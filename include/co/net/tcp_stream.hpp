#pragma once

#include <cassert>
#include <co/event.hpp>
#include <co/net/error_code.hpp>
#include <co/result.hpp>
#include <co/scheduler.hpp>
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

class tcp_listener;

class tcp_stream
{
    friend tcp_listener;

private:
    tcp_stream(impl::uv_tcp_ptr tcp_ptr)
        : _tcp_ptr(std::move(tcp_ptr))
    {}

public:
    static func<result<tcp_stream>> connect(const std::string& ip, uint16_t port);

    tcp_stream(const tcp_stream&) = delete;
    tcp_stream& operator=(const tcp_stream&) = delete;

    tcp_stream(tcp_stream&& other) = default;
    tcp_stream& operator=(tcp_stream&& other) = default;

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

func<result<tcp_stream>> tcp_stream::connect(const std::string& ip, uint16_t port)
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

    co_return co::ok(tcp_stream{ std::move(tcp_ptr) });
}

}  // namespace co::net
