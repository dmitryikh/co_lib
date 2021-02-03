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
    static func<connection> connect(const std::string& ip, uint16_t port)
    {
        co_return connection{ co_await co::net::connect(ip, port) };
    };

    connection(co::net::tcp socket)
        : _socket(std::move(socket))
    {
        _write_buffer.reserve(WRITE_BUFFER);
    }

    func<result<void>> write(const command& cmd)
    {
        if (!_write_buffer.empty())
        {
            const size_t reserve = _write_buffer.capacity() - _write_buffer.size();
            if (cmd.bytes_estimation() > reserve)
            {
                BOOST_OUTCOME_CO_TRYV(co_await flush());
            }
        }

        cmd.serialize(_write_buffer);
        co_return co::ok();
    }

    func<result<void>> flush()
    {
        auto res = co_await _socket.write(_write_buffer.data(), _write_buffer.size());
        _write_buffer.clear();
        co_return res;
    }

    func<result<reply>> read()
    {
        CO_RESULT_TRY(type_id, co_await read_char());
        switch (type_id)
        {
            case '+':
            {
                CO_RESULT_TRY(reply, co_await read_string());
                co_return co::ok(reply_string{ std::move(reply) });
            }
            case '-':
            {
                CO_RESULT_TRY(reply, co_await read_string());
                co_return co::ok(reply_error{ std::move(reply) });
            }
            case ':':
            {
                CO_RESULT_TRY(num, co_await read_int());
                co_return co::ok(num);
            }
            case '$':
                co_return co_await read_bulk_string();
            case '*':
                co_return co_await read_array();
        }
        co_return co::err(unknown_data_type);
    }

    func<void> shutdown()
    {
        auto _ = co_await _socket.shutdown();
    }

private:
    func<result<std::string>> read_string()
    {
        CO_RESULT_TRY(len, co_await read_until('\r'));
        CO_RESULT_TRYV(co_await read_n(len + 2));

        assert(_read_buffer.size() >= len + 2);

        auto reply = std::string{ _read_buffer.begin(), _read_buffer.begin() + len };
        _read_buffer.erase(_read_buffer.begin(), _read_buffer.begin() + len + 2);

        co_return reply;
    }

    func<result<std::string>> read_string_n(size_t n)
    {
        CO_RESULT_TRYV(co_await read_n(n + 2));

        assert(_read_buffer.size() >= n + 2);

        if (_read_buffer[n] != '\r' || _read_buffer[n + 1] != '\n')
            co_return co::err(protocol_error);

        auto reply = std::string{ _read_buffer.begin(), _read_buffer.begin() + n };
        _read_buffer.erase(_read_buffer.begin(), _read_buffer.begin() + n + 2);

        co_return reply;
    }


    func<result<int64_t>> read_int()
    {
        CO_RESULT_TRY(len, co_await read_until('\r'));
        CO_RESULT_TRYV(co_await read_n(len + 2));

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
                co_return co::err(protocol_error);

            res = res * 10;
            res += _read_buffer[pos] - '0';
            pos++;
        }

        _read_buffer.erase(_read_buffer.begin(), _read_buffer.begin() + len + 2);

        co_return res * sign;
    }

    func<result<reply>> read_bulk_string()
    {
        CO_RESULT_TRY(len, co_await read_int());
        if (len == -1)
            co_return co::ok(reply_null{});

        if (len < 0)
            co_return co::err(protocol_error);

        CO_RESULT_TRY(str, co_await read_string_n(static_cast<size_t>(len)));
        co_return co::ok(reply_bulk_string{ std::move(str) });
    }

    func<result<reply>> read_array()
    {
        CO_RESULT_TRY(len, co_await read_int());
        if (len == -1)
            co_return co::ok(reply_null{});

        if (len < 0)
            co_return co::err(protocol_error);

        reply_array array;
        array.reserve(len);

        for (size_t i = 0; i < len; i++)
        {
            CO_RESULT_TRY(repl, co_await read());
            array.push_back(std::move(repl));
        }
        co_return co::ok(array);
    }

    func<result<char>> read_char()
    {
        CO_RESULT_TRYV(co_await read_n(1));
        const char c = _read_buffer[0];
        _read_buffer.pop_front();
        co_return c;
    }

    func<result<size_t>> read_until(char until)
    {
        auto it = _read_buffer.begin();
        while (true)
        {
            if (it == _read_buffer.end())
            {
                CO_RESULT_TRYV(co_await read_to_buffer());
            }
            if (*it ==  until)
                co_return std::distance(_read_buffer.begin(), it);
            ++it;
        }
    }

    func<result<size_t>> read_n(size_t n)
    {
        while (_read_buffer.size() < n)
        {
            BOOST_OUTCOME_CO_TRYV(co_await read_to_buffer());
        }

        assert(_read_buffer.size() >= n);
        co_return n;
    }

    // func<void> skip_n(size_t n)
    // {
    //     co_await read_n(n);
    //     assert(_read_buffer.size() >= n);
    //     _read_buffer.erase(_read_buffer.begin(), _read_buffer.begin() + n);
    // }

    func<result<void>> read_to_buffer()
    {
        std::array<char, 16 * 1024> buffer;
        auto len = co_await _socket.read(buffer.data(), buffer.size());
        if (!len && len.assume_error() == co::net::eof)
        {
            if (len.assume_error() == co::net::eof)
            {
                // connection is closed by foreign host
                co_await shutdown();
            }
            co_return len.as_failure();
        }

        // TODO: officially redis supports mesasges up to 512MB
        if (_read_buffer.size() + len.value() > READ_BUFFER_MAX)
        {
            // TODO: redis error codes
            co_return buffer_overflow;
        }

        _read_buffer.insert(_read_buffer.end(), buffer.begin(), buffer.begin() + len.value());
        co_return co::ok();
    }

private:
    co::net::tcp _socket;
    std::deque<char> _read_buffer;
    std::vector<char> _write_buffer;
};

}  // namespace co::redis