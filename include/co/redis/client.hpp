#pragma once

#include <boost/circular_buffer.hpp>
#include <co/channel.hpp>
#include <co/future.hpp>
#include <co/redis/command.hpp>
#include <co/redis/connection.hpp>
#include <co/sleep.hpp>
#include <co/stop_token.hpp>
#include <co/thread.hpp>

namespace co::redis
{

namespace impl
{

class request_command
{
public:
    request_command(command&& cmd)
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

class request_flush
{};

using request = std::variant<request_command, request_flush>;

class connection_processor
{
    static constexpr size_t in_fly_capacity = 1024;

public:
    connection_processor(connection&& conn, co::channel<request>& in_ch)
        : _conn(std::move(conn))
        , _in_fly(in_fly_capacity)
        , _write_loop(write_loop_func(in_ch), "write_loop")
        , _read_loop(read_loop_func(), "read_loop")
    {}

    co::func<co::result<void>> join(const co::stop_token& stop)
    {
        auto callback = co::stop_callback(stop,
                                          [this]()
                                          {
                                              _write_loop.request_stop();
                                              _read_loop.request_stop();
                                              if (!_result.is_err())
                                                  _result = co::err(co::cancel);
                                          });

        co_await _write_loop.join();
        co_await _read_loop.join();

        if (_result.is_ok() && !_in_fly.empty())
            throw co::exception(co::other, "logic error: result is ok but in fly queue is not empty");

        while (!_in_fly.empty())
        {
            auto& request = _in_fly.front();
            request.set_reply(_result.err());
            _in_fly.pop_front();
        }

        co_return _result;
    }

private:
    co::func<void> write_loop_func(co::channel<request>& in_ch)
    {
        try
        {
            auto token = co::this_thread::get_stop_token();
            while (!token.stop_requested())
            {
                auto req_r = co_await in_ch.pop(token);
                if (req_r == co::cancel)
                    break;
                if (req_r == co::closed)
                    break;

                auto req = std::move(req_r.unwrap());

                if (std::holds_alternative<request_command>(req))
                {
                    auto& req_cmd = std::get<request_command>(req);
                    auto cmd = std::move(req_cmd.cmd);

                    if (_in_fly.size() >= in_fly_capacity)
                    {
                        co_await _conn.flush().unwrap();
                        co_await _in_fly_cv.wait([&]() { return _in_fly.size() < in_fly_capacity; });
                    }

                    _in_fly.push_back(std::move(req_cmd));
                    co_await _conn.write(std::move(cmd)).unwrap();
                }
                else if (std::holds_alternative<request_flush>(req))
                {
                    co_await _conn.flush().unwrap();
                }
                else
                {
                    assert(false);  // unreachable
                }
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
                _in_fly.pop_front();
                _in_fly_cv.notify_one();
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
    boost::circular_buffer<request_command> _in_fly;
    co::condition_variable _in_fly_cv;
    co::result<void> _result = co::ok();

    co::thread _write_loop;
    co::thread _read_loop;
};

}  // namespace impl

class client
{
public:
    client(const std::string& ip, uint16_t port)
        : _ip(ip)
        , _port(port)
        , _req_channel(1024)
        , _reconnect_loop_thread(reconnect_loop_func())
    {}

    void close()
    {
        _req_channel.close();
    }

    void stop()
    {
        _stop_source.request_stop();
    }

    co::func<void> join()
    {
        co_await _reconnect_loop_thread.join();
    }

    co::func<co::future<co::result<reply>>> get(const std::string& key)
    {
        return send_command({ { "GET", key } });
    }

    co::func<co::future<co::result<reply>>> set(const std::string& key, const std::string& value)
    {
        return send_command({ { "SET", key, value } });
    }

    bool is_connected() const
    {
        return _is_connected;
    }

    co::func<void> wait_until_connected()
    {
        co_await _is_connected_cv.wait([&]() { return _is_connected; });
    }

    co::func<void> flush()
    {
        auto _ = co_await _req_channel.push(impl::request_flush{});
    }

private:
    co::func<co::future<co::result<reply>>> send_command(command cmd)
    {
        auto req = impl::request_command{ std::move(cmd) };
        auto future = req.get_future();
        if (_stop_source.stop_requested())
        {
            req.set_reply(co::err(co::cancel));
            co_return future;
        }
        if (!_is_connected)
        {
            req.set_reply(co::err(co::closed));
            co_return future;
        }
        auto res = co_await _req_channel.push(std::move(req));
        if (res.is_err())
            req.set_reply(res.err());

        co_return std::move(future);
    }

    co::func<void> reconnect_loop_func()
    {
        auto token = _stop_source.get_token();
        while (!token.stop_requested() && !_req_channel.is_closed())
        {
            try
            {
                auto con = co_await co::redis::connection::connect(_ip, _port);
                auto coproc = impl::connection_processor(std::move(con.unwrap()), _req_channel);
                _is_connected = true;
                _is_connected_cv.notify_all();
                co_await coproc.join(token).unwrap();
            }
            catch (const co::exception& coexc)
            {
                std::cerr << "redis:client: " << coexc << "\n";
            }
            _is_connected = false;
            if (!token.stop_requested() && !_req_channel.is_closed())
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
    bool _is_connected = false;
    co::condition_variable _is_connected_cv;
    co::stop_source _stop_source;
    co::channel<impl::request> _req_channel;
    co::thread _reconnect_loop_thread;
};

}  // namespace co::redis
