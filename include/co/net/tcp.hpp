#pragma once

#include <cassert>
#include <co/event.hpp>
#include <co/net/error_code.hpp>
#include <co/result.hpp>
#include <co/scheduler.hpp>
#include <co/std.hpp>
#include <uv.h>

namespace co::net
{

namespace impl
{

inline void on_close(uv_handle_t* handle)
{
    assert(handle != nullptr);
    delete handle;
}

void close_tcp_handle(uv_tcp_t* handle)
{
    uv_close((uv_handle_t*)handle, on_close);
}

using uv_tcp_ptr = std::unique_ptr<uv_tcp_t, decltype(&close_tcp_handle)>;

uv_tcp_ptr make_and_init_uv_tcp_handle()
{
    uv_tcp_t* handle = new uv_tcp_t;
    uv_tcp_init(co::impl::get_scheduler().uv_loop(), handle);
    return uv_tcp_ptr{ handle, close_tcp_handle };
}

}  // namespace impl

class accept;
class accept_shared_state;

class tcp
{
    friend accept;
    friend accept_shared_state;

private:
    tcp(impl::uv_tcp_ptr tcp_ptr)
        : _tcp_ptr(std::move(tcp_ptr))
    {}

public:
    static func<result<tcp>> connect(const std::string& ip, uint16_t port);
    static func<result<accept>> accept(const std::string& ip, uint16_t port);

    tcp(const tcp&) = delete;
    tcp& operator=(const tcp&) = delete;

    tcp(tcp&& other) = default;
    tcp& operator=(tcp&& other) = default;

    func<result<size_t>> read(char* data, size_t len)
    {
        struct read_state
        {
            char* buffer_ptr;
            size_t buffer_len;
            int read_status;
            size_t read_len;
            event& ev;
        };

        event ev;

        auto state = read_state{ data, len, 0, 0, ev };

        auto alloc = [](uv_handle_t* handle, size_t /*suggested_size*/, uv_buf_t* buf)
        {
            assert(handle != nullptr);
            assert(handle->data != nullptr);

            auto& state = *static_cast<read_state*>(handle->data);
            buf->base = state.buffer_ptr;
            buf->len = state.buffer_len;
        };

        auto on_read = [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* /*buf*/)
        {
            assert(stream != nullptr);
            assert(stream->data != nullptr);

            auto& state = *static_cast<read_state*>(stream->data);

            if (nread == 0)
            {
                // nread might be 0, which does not indicate an error or EOF. This
                // is equivalent to EAGAIN or EWOULDBLOCK under read(2).
                return;
            }
            if (nread == UV_EOF)
                state.read_len = 0;
            else if (nread < 0)
                state.read_status = -nread;
            else if (nread > 0)
                state.read_len = static_cast<size_t>(nread);

            const int ret = uv_read_stop(stream);
            if (ret != 0)
                throw std::runtime_error("uv_read_stop failed");

            state.ev.notify();
        };

        assert(_tcp_ptr);
        _tcp_ptr->data = static_cast<void*>(&state);
        int ret = uv_read_start((uv_stream_t*)_tcp_ptr.get(), alloc, on_read);
        using namespace std::string_literals;
        if (ret != 0)
            co_return co::err(other_net);

        co_await ev.wait();

        if (state.read_status != 0)
            co_return co::err(other_net);

        if (state.read_len == 0)
            co_return co::err(eof);

        co_return co::ok(state.read_len);
    }

    func<result<void>> write(const char* data, size_t len)
    {
        struct write_state
        {
            int status;
            event& ev;
        };

        event ev;
        std::array<uv_buf_t, 1> bufs = { uv_buf_init(const_cast<char*>(data), len) };

        auto state = write_state{ 0, ev };

        auto on_write = [](uv_write_t* handle, int status)
        {
            assert(handle != nullptr);
            assert(handle->data != nullptr);

            auto& state = *static_cast<write_state*>(handle->data);
            state.status = status;
            state.ev.notify();
        };

        uv_write_t write_handle;
        write_handle.data = static_cast<void*>(&state);
        assert(_tcp_ptr);
        int ret =
            uv_write((uv_write_t*)&write_handle, (uv_stream_t*)_tcp_ptr.get(), bufs.data(), bufs.size(), on_write);
        using namespace std::string_literals;
        if (ret != 0)
            co_return co::err(other_net);

        co_await ev.wait();

        if (state.status != 0)
            co_return co::err(other_net);

        co_return co::ok();
    }

    func<result<void>> shutdown()
    {
        struct shutdown_state
        {
            int status;
            event& ev;
        };

        event ev;

        auto state = shutdown_state{ 0, ev };

        auto on_shutdown = [](uv_shutdown_t* handle, int status)
        {
            assert(handle != nullptr);
            assert(handle->data != nullptr);

            auto& state = *static_cast<shutdown_state*>(handle->data);
            state.status = status;
            state.ev.notify();
        };

        uv_shutdown_t shutdown_handle;
        shutdown_handle.data = static_cast<void*>(&state);
        assert(_tcp_ptr);
        const int ret = uv_shutdown(&shutdown_handle, (uv_stream_t*)_tcp_ptr.get(), on_shutdown);
        using namespace std::string_literals;
        if (ret != 0)
            co_return co::err(other_net);

        co_await ev.wait();

        if (state.status != 0)
            co_return co::err(other_net);

        co_return co::ok();
    }

private:
    impl::uv_tcp_ptr _tcp_ptr;
};

func<result<tcp>> tcp::connect(const std::string& ip, uint16_t port)
{
    struct connect_state
    {
        int status;
        event& ev;
    };

    event ev;

    auto state = connect_state{ 0, ev };

    auto on_connect = [](uv_connect_t* connect, int status)
    {
        assert(connect != nullptr);
        assert(connect->data != nullptr);

        auto& state = *static_cast<connect_state*>(connect->data);
        state.status = status;
        state.ev.notify();
    };

    struct sockaddr_in dest;
    int ret = uv_ip4_addr(ip.c_str(), port, &dest);
    using namespace std::string_literals;
    if (ret != 0)
        co_return co::err(wrong_address);

    auto tcp_ptr = impl::make_and_init_uv_tcp_handle();
    uv_connect_t connect;
    connect.data = static_cast<void*>(&state);

    ret = uv_tcp_connect(&connect, tcp_ptr.get(), (const struct sockaddr*)&dest, on_connect);
    if (ret != 0)
        co_return co::err(other_net);

    co_await ev.wait();

    if (state.status != 0)
        co_return co::err(other_net);

    co_return co::ok(tcp{ std::move(tcp_ptr) });
}

namespace impl
{

struct connection_msg
{};
struct error_msg
{
    int status;
};
using message = std::variant<connection_msg, error_msg>;

struct accept_shared_state
{
    accept_shared_state(uv_tcp_ptr&& tcp_ptr)
        : _server_tcp_ptr(std::move(tcp_ptr))
    {}

    bool _stopping = false;
    impl::uv_tcp_ptr _server_tcp_ptr;
    std::queue<message> _queue;
    co::condition_variable _cv;
};

using accept_shared_state_ptr = std::unique_ptr<accept_shared_state>;

}  // namespace impl

class accept
{
    friend class tcp;
    static constexpr int INCOMING_MAX = 128;

private:
    accept(impl::uv_tcp_ptr tcp_ptr, const sockaddr_in& addr)
        : _state(std::make_unique<impl::accept_shared_state>(std::move(tcp_ptr)))
    {
        assert(_state->_server_tcp_ptr != nullptr);

        auto ret = uv_tcp_bind(_state->_server_tcp_ptr.get(), (const struct sockaddr*)&addr, 0);
        if (ret != 0)
            throw co::exception(other_net);

        _state->_server_tcp_ptr->data = static_cast<void*>(_state.get());
        ret = uv_listen((uv_stream_t*)_state->_server_tcp_ptr.get(), INCOMING_MAX, on_new_connection);
        if (ret != 0)
            throw co::exception(other_net, uv_strerror(ret));
    }

public:
    accept(const accept&) = delete;
    accept& operator=(const accept&) = delete;

    accept(accept&& other) = default;
    accept& operator=(accept&& other) = default;

    func<result<tcp>> next(co::until until = {})
    {
        while (_state->_queue.empty() && !_state->_stopping)
        {
            auto res = co_await _state->_cv.wait(until);
            if (res.is_err())
                co_return res.err();
        }

        if (_state->_stopping)
            co_return co::err(co::closed);

        assert(!_state->_queue.empty());
        auto& msg = _state->_queue.front();
        _state->_queue.pop();

        if (std::holds_alternative<impl::connection_msg>(msg))
        {
            auto client_tcp_ptr = impl::make_and_init_uv_tcp_handle();

            auto res = uv_accept((uv_stream_t*)_state->_server_tcp_ptr.get(), (uv_stream_t*)client_tcp_ptr.get());
            if (res != 0)
                co_return co::err(other_net, uv_strerror(res));

            co_return co::ok(tcp{ std::move(client_tcp_ptr) });
        }
        else if (std::holds_alternative<impl::error_msg>(msg))
        {
            const impl::error_msg err_msg = std::get<impl::error_msg>(msg);
            co_return co::err(other_net, uv_strerror(err_msg.status));
        }
        else
        {
            assert(false);  // unreachable
        }
    }

private:
    static void on_new_connection(uv_stream_t* server, int status)
    {
        assert(server != nullptr);
        assert(server->data != nullptr);

        auto& state = *static_cast<impl::accept_shared_state*>(server->data);

        if (status < 0)
            state._queue.push(impl::error_msg{ status });
        else
            state._queue.push(impl::connection_msg{});
        state._cv.notify_one();
    }

private:
    impl::accept_shared_state_ptr _state;
};

func<result<accept>> tcp::accept(const std::string& ip, uint16_t port)
{

    struct sockaddr_in addr;
    int ret = uv_ip4_addr(ip.c_str(), port, &addr);
    if (ret != 0)
        co_return co::err(wrong_address);

    try
    {
        auto acc = co::net::accept(impl::make_and_init_uv_tcp_handle(), addr);
        co_return co::ok(std::move(acc));
    }
    catch (const co::exception& coexc)
    {
        co_return co::err(coexc);
    }
}

}  // namespace co::net
