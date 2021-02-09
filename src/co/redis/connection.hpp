#pragma once

#include <charconv>
#include <co/func.hpp>
#include <co/net/network.hpp>
#include <co/redis/reply.hpp>
#include <co/redis/command.hpp>
#include <co/redis/error_code.hpp>

namespace co::redis
{

class connection
{
    constexpr static size_t READ_BUFFER_MAX = 1 * 1024 * 1024;
    constexpr static size_t WRITE_BUFFER = 64 * 1024;
public:
    static func<result<connection>> connect(const std::string& ip, uint16_t port)
    {
        auto tcp = co_await co::net::connect(ip, port);
        if (tcp.is_err())
            co_return tcp.err();
        co_return co::ok(connection{ std::move(tcp.unwrap()) });
    };

    connection(co::net::tcp socket)
        : _socket(std::move(socket))
    {
        _write_buffer.reserve(WRITE_BUFFER);
    }

    func<result<void>> write(const command& cmd)
    {
        try
        {
            if (!_write_buffer.empty())
            {
                const size_t reserve = _write_buffer.capacity() - _write_buffer.size();
                if (cmd.bytes_estimation() > reserve)
                {
                    co_await flush().unwrap();
                }
            }

            cmd.serialize(_write_buffer);
            co_return co::ok();
        }
        catch (const co::exception& coexc)
        {
            co_return co::err(coexc);
        }
    }

    func<result<void>> flush()
    {
        if (_write_buffer.empty())
            co_return co::ok();

        auto res = co_await _socket.write(_write_buffer.data(), _write_buffer.size());
        _write_buffer.clear();
        co_return res;
    }

    func<result<reply>> read()
    {
        try
        {
            const char type_id = co_await read_char();
            switch (type_id)
            {
                case '+':
                    co_return co::ok(reply_string{ co_await read_string() });
                case '-':
                    co_return co::ok(reply_error{ co_await read_string() });
                case ':':
                    co_return co::ok(co_await read_int());
                case '$':
                    co_return co::ok(co_await read_bulk_string());
                case '*':
                    co_return co::ok(co_await read_array());
            }
                co_return co::err(unknown_data_type);
        }
        catch (const co::exception& coexc)
        {
            co_return co::err(coexc);  // implicitly convert co::exception to co::result with error inside
        }
    }

    func<void> shutdown()
    {
        auto res = co_await _socket.shutdown();
    }

private:
    func<std::string> read_string()
    {
        const size_t len = co_await read_until('\r');
        co_await read_n(len + 2);

        assert(_read_buffer.size() >= len + 2);

        auto reply = std::string{ _read_buffer.begin(), _read_buffer.begin() + len };
        _read_buffer.erase(_read_buffer.begin(), _read_buffer.begin() + len + 2);

        co_return reply;
    }

    func<std::string> read_string_n(size_t n)
    {
        co_await read_n(n + 2);

        assert(_read_buffer.size() >= n + 2);

        if (_read_buffer[n] != '\r' || _read_buffer[n + 1] != '\n')
            throw co::exception(protocol_error, "expecting \\r\\n while reading fixed lenght string");

        auto reply = std::string{ _read_buffer.begin(), _read_buffer.begin() + n };
        _read_buffer.erase(_read_buffer.begin(), _read_buffer.begin() + n + 2);

        co_return reply;
    }


    func<int64_t> read_int()
    {
        const size_t len = co_await read_until('\r');
        co_await read_n(len + 2);

        assert(_read_buffer.size() >= len + 2);

        // NOTE: can't use std::from_chars here because _read_buffer is not contigious
        size_t pos = 0;
        int64_t sign = 1;
        int64_t res = 0;
        if (_read_buffer[pos] == '-')
        {
            pos++;
            sign = -1;
        }

        while (pos < len)
        {
            if (!std::isdigit(_read_buffer[pos]))
                throw co::exception(protocol_error, "integer contains no digit symbol");

            res = res * 10;
            res += _read_buffer[pos] - '0';
            pos++;
        }

        _read_buffer.erase(_read_buffer.begin(), _read_buffer.begin() + len + 2);

        co_return res * sign;
    }

    func<reply> read_bulk_string()
    {
        const int64_t len = co_await read_int();
        if (len == -1)
            co_return reply_null{};

        if (len < 0)
            throw co::exception(protocol_error, "got negative length while parsing bulk string");

        auto str = co_await read_string_n(static_cast<size_t>(len));
        co_return reply_bulk_string{ std::move(str) };
    }

    func<reply> read_array()
    {
        const int64_t len = co_await read_int();
        if (len == -1)
            co_return reply_null{};

        if (len < 0)
            throw co::exception(protocol_error, "got negative length while parsing array");

        reply_array array;
        array.reserve(len);

        for (size_t i = 0; i < len; i++)
        {
            array.push_back(co_await read().unwrap());
        }
        co_return array;
    }

    func<char> read_char()
    {
        co_await read_n(1);
        const char c = _read_buffer[0];
        _read_buffer.pop_front();
        co_return c;
    }

    func<size_t> read_until(char until)
    {
        auto it = _read_buffer.begin();
        while (true)
        {
            if (it == _read_buffer.end())
            {
                co_await read_to_buffer();
            }
            if (*it ==  until)
                co_return std::distance(_read_buffer.begin(), it);
            ++it;
        }
    }

    func<size_t> read_n(size_t n)
    {
        while (_read_buffer.size() < n)
        {
            co_await read_to_buffer();
        }

        assert(_read_buffer.size() >= n);
        co_return n;
    }

    func<void> read_to_buffer()
    {
        std::array<char, 16 * 1024> buffer;
        const auto res = co_await _socket.read(buffer.data(), buffer.size());
        if (res == co::net::eof)
        {
            // connection is closed by foreign host
            co_await shutdown();
        }

        const size_t len = res.unwrap();  // will throw the error as exception

        // TODO: officially redis supports mesasges up to 512MB
        if (_read_buffer.size() + len > READ_BUFFER_MAX)
            throw co::exception(buffer_overflow, "read buffer exceeds its size");

        _read_buffer.insert(_read_buffer.end(), buffer.begin(), buffer.begin() + len);
    }

private:
    co::net::tcp _socket;
    std::deque<char> _read_buffer;
    std::vector<char> _write_buffer;
};

}  // namespace co::redis