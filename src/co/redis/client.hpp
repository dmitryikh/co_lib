#pragma once

#include <co/channel.hpp>
#include <co/future.hpp>
#include <co/sleep.hpp>
#include <co/stop_token.hpp>
#include <co/thread.hpp>
#include <co/redis/connection.hpp>
#include <co/redis/command.hpp>

namespace co::redis
{

namespace impl
{

class request
{
public:
    request(command&& cmd)
        : cmd(std::move(cmd))
    {}

    co::future<co::result<reply>> get_future() const
    {
        return _promise.get_future();
    }

    void set_reply(co::result<reply>&& reply)
    {
        _promise.set_value(std::move(reply));
    }

    command cmd;
    co::promise<co::result<reply>> _promise;
};

class flush_message
{};

class connection_processor
{
public:
    connection_processor(connection&& conn, co::channel<request>& in, co::channel<flush_message>& flush_ch)
        : _conn(std::move(conn))
        , _write_loop(write_loop_func(in))
        , _read_loop(read_loop_func())
        , _flush_loop(flush_loop_func(flush_ch))
    {}

    co::func<co::result<void>> join(const co::stop_token& stop)
    {
        auto callback = co::stop_callback(stop, [this] () {
            _write_loop.request_stop();
            _read_loop.request_stop();
            if (!_result.is_err())
                _result = co::err(co::cancel);
        });

        co_await _write_loop.join();
        co_await _read_loop.join();
        co_await _flush_loop.join();

        if (_result.is_ok() && !_in_fly.empty())
            throw co::exception(co::other, "logic error: result is ok but in fly queue is not empty");

        while (!_in_fly.empty())
        {
            auto& request = _in_fly.front();
            request.set_reply(_result.err());
            _in_fly.pop();
        }

        co_return  _result;
    }

private:
    co::func<void> write_loop_func(co::channel<request>& in)
    {
        try
        {
            auto token = co::this_thread::get_stop_token();

            while (!token.stop_requested())
            {
                auto req = co_await in.pop(token);
                if (req == co::cancel)
                    break;
                if (req == co::closed)
                    break;

                auto cmd = std::move(req.unwrap().cmd);
                _in_fly.push(std::move(req.unwrap()));
                (co_await _conn.write(std::move(cmd))).unwrap();
            }
        }
        catch (const co::exception& coexc)
        {
            if (!_result.is_err())
                _result = coexc.errc();
        }
        co_await _conn.shutdown();
    }

    co::func<void> read_loop_func()
    {
        try
        {
            auto token = co::this_thread::get_stop_token();
            while (!token.stop_requested())
            {
                auto reply = co_await _conn.read();
                if (reply.is_err())
                    throw co::exception(reply.err());

                if (_in_fly.empty())
                    throw co::exception(co::redis::protocol_error, "can't find request for reply");

                auto& request = _in_fly.front();
                request.set_reply(co::ok(std::move(reply.unwrap())));
                _in_fly.pop();
            }
        }
        catch (const co::exception& coexc)
        {
            if (!_result.is_err())
                _result = coexc.errc();
        }
        co_await _conn.shutdown();
    }

    co::func<void> flush_loop_func(co::channel<flush_message>& flush_ch)
    {
        using namespace std::chrono_literals;
        const auto period = 15ms;
        try
        {
            auto token = co::this_thread::get_stop_token();

            while (!token.stop_requested())
            {
                auto res = co_await flush_ch.pop_for(period, token);
                if (res.is_err())
                    std::cout << "\n res = " << res.err() << "\n";
                if (res == co::cancel)
                    break;
                (co_await _conn.flush()).unwrap();
            }
        }
        catch (const co::exception& coexc)
        {
            if (!_result.is_err())
                _result = coexc.errc();
        }
        co_await _conn.shutdown();
    }
private:
    connection _conn;
    std::queue<request> _in_fly;
    co::result<void> _result = co::ok();

    co::thread _write_loop;
    co::thread _read_loop;
    co::thread _flush_loop;
};

}  // namespace impl

class client
{
public:
    client(const std::string& ip, uint16_t port)
        : _ip(ip)
        , _port(port)
        , _req_channel(1024)
        , _flush_channel(2)
        , _reconnect_loop_thread(reconnect_loop_func())
    {}

    void stop()
    {
        _stop_source.request_stop();
    }

    co::func<void> join()
    {
        co_await _reconnect_loop_thread.join();
    }

    co::future<co::result<reply>> get(const std::string& key)
    {
        return send_command({{ "GET", key }});
    }

    co::future<co::result<reply>> set(const std::string& key, const std::string& value)
    {
        return send_command({{ "SET", key, value }});
    }

    bool is_connected() const
    {
        return _is_connected;
    }

    void flush()
    {
        _flush_channel.try_push(impl::flush_message{});
    }

private:
    co::future<co::result<reply>> send_command(command&& cmd)
    {
        auto req = impl::request{ std::move(cmd) };
        auto future = req.get_future();
        if (_stop_source.stop_requested())
        {
            req.set_reply(co::err(co::cancel));
            return future;
        }
        if (!_is_connected)
        {
            req.set_reply(co::err(co::closed));
            return future;
        }
        auto res = _req_channel.try_push(std::move(req));
        if (res.is_err())
            req.set_reply(res.err());

        return future;
    }

    co::func<void> reconnect_loop_func()
    {
        auto token = _stop_source.get_token();
        while (!token.stop_requested())
        {
            try
            {
                auto con = co_await co::redis::connection::connect(_ip, _port);
                auto coproc = impl::connection_processor(std::move(con.unwrap()), _req_channel, _flush_channel);
                _is_connected = true;
                (co_await coproc.join(token)).unwrap();
            }
            catch (const co::exception& coexc)
            {
                std::cerr << "redis:client: " << coexc << "\n";
            }
            _is_connected = false;
            if (!token.stop_requested())
            {
                // wait before reconnect
                using namespace std::chrono_literals;
                co_await co::this_thread::sleep_for(1s);
            }
        }
    }

private:
    const std::string _ip;
    const uint16_t _port;
    co::stop_source _stop_source;
    co::channel<impl::request> _req_channel;
    co::thread _reconnect_loop_thread;
};

}  // namespase co::redis
