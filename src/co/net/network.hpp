#pragma once

#include <experimental/coroutine>
#include <uv.h>
#include <co/base/scheduler.hpp>

namespace co::net
{

using tcp_uv_ptr = std::unique_ptr<uv_tcp_t>;

class awaitable_read
{
public:
    explicit awaitable_read(uv_tcp_t& tcp_handle, char* buffer_ptr, size_t buffer_len)
        : _tcp_handle(tcp_handle)
        , _buffer_ptr(buffer_ptr)
        , _buffer_len(buffer_len)
    {
        assert(buffer_ptr != nullptr);
        assert(buffer_len > 0);
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::experimental::coroutine_handle<> awaiting_coroutine) noexcept
    {
        _coro = awaiting_coroutine;
        _tcp_handle.data = static_cast<void*>(this);
        int ret = uv_read_start((uv_stream_t*)&_tcp_handle, alloc, on_read);
        if (ret != 0)
        {
            _status = ret;
            co::base::get_scheduler().ready(awaiting_coroutine);
        }
    }

    size_t await_resume()
    {
        using namespace std::string_literals;

        if (_status != 0)
            throw std::runtime_error("read error: "s + uv_strerror(_status));

        return _read_len;
    }

private:
    static void alloc(uv_handle_t* handle, size_t /*suggested_size*/, uv_buf_t* buf)
    {
        assert(handle != nullptr);
        assert(handle->data != nullptr);

        auto& self = *static_cast<awaitable_read*>(handle->data);
        buf->base = self._buffer_ptr;
        buf->len = self._buffer_len;
    }

    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* /*buf*/)
    {
        assert(stream != nullptr);
        assert(stream->data != nullptr);

        auto& self = *static_cast<awaitable_read*>(stream->data);
        self._status = 0;
        self._read_len = 0;

        if (nread == 0)
        {
            // nread might be 0, which does not indicate an error or EOF. This
            // is equivalent to EAGAIN or EWOULDBLOCK under read(2).
            return;
        }
        if (nread == UV_EOF)
            self._read_len = 0;
        else if (nread < 0)
            self._status = -nread;
        else if (nread > 0)
            self._read_len = static_cast<size_t>(nread);

        /*int ret = */uv_read_stop(stream);
        co::base::get_scheduler().ready(self._coro);
    }

private:
    int _status = 0;
    size_t _read_len = 0;
    std::experimental::coroutine_handle<> _coro;
    uv_tcp_t& _tcp_handle;
    char* _buffer_ptr;
    size_t _buffer_len;
};

class awaitable_write
{
public:
    explicit awaitable_write(uv_tcp_t& tcp_handle, const char* buffer_ptr, size_t buffer_len)
        : _tcp_handle(tcp_handle)
        , _bufs{ uv_buf_init(const_cast<char*>(buffer_ptr), buffer_len) }
    {
        assert(buffer_ptr != nullptr);
        assert(buffer_len > 0);
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::experimental::coroutine_handle<> awaiting_coroutine) noexcept
    {
        _coro = awaiting_coroutine;
        _write_handle.data = static_cast<void*>(this);
        int ret = uv_write((uv_write_t*)&_write_handle, (uv_stream_t*)&_tcp_handle, _bufs.data(), _bufs.size(), on_write);
        if (ret != 0)
        {
            _status = ret;
            co::base::get_scheduler().ready(awaiting_coroutine);
        }
    }

    void await_resume()
    {
        using namespace std::string_literals;

        if (_status != 0)
            throw std::runtime_error("read error: "s + uv_strerror(_status));
    }

private:
    static void on_write(uv_write_t* handle, int status)
    {
        assert(handle != nullptr);
        assert(handle->data != nullptr);

        auto& self = *static_cast<awaitable_write*>(handle->data);
        self._status = status;
        co::base::get_scheduler().ready(self._coro);
    }

private:
    int _status = 0;
    std::experimental::coroutine_handle<> _coro;
    uv_write_t _write_handle;
    uv_tcp_t& _tcp_handle;
    std::array<uv_buf_t, 1> _bufs;
};

class awaitable_shutdown
{
public:
    explicit awaitable_shutdown(uv_tcp_t& tcp_handle)
        : _tcp_handle(tcp_handle)
    {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::experimental::coroutine_handle<> awaiting_coroutine) noexcept
    {
        _coro = awaiting_coroutine;
        _shutdown_handle.data = static_cast<void*>(this);
        int ret = uv_shutdown(&_shutdown_handle, (uv_stream_t*)&_tcp_handle, on_shutdown);
        if (ret != 0)
        {
            _status = ret;
            co::base::get_scheduler().ready(awaiting_coroutine);
        }
    }

    void await_resume()
    {
        using namespace std::string_literals;

        if (_status != 0)
            throw std::runtime_error("shutdown error: "s + uv_strerror(_status));
    }

private:
    static void on_shutdown(uv_shutdown_t* handle, int status)
    {
        assert(handle != nullptr);
        assert(handle->data != nullptr);

        auto& self = *static_cast<awaitable_shutdown*>(handle->data);
        self._status = status;
        co::base::get_scheduler().ready(self._coro);
    }

private:
    int _status = 0;
    uv_shutdown_t _shutdown_handle;
    std::experimental::coroutine_handle<> _coro;
    uv_tcp_t& _tcp_handle;
};

class tcp
{
public:
    tcp(tcp_uv_ptr tcp_ptr)
        : _tcp_ptr(std::move(tcp_ptr))
    {}

    tcp(const tcp&) = delete;
    tcp& operator=(const tcp&) = delete;

    tcp(tcp&& other)
    : tcp(std::move(other._tcp_ptr))
    {}

    awaitable_read read(char* data, size_t size)
    {
        return awaitable_read(*_tcp_ptr.get(), data, size);
    }

    awaitable_write write(const char* data, size_t size)
    {
        return awaitable_write(*_tcp_ptr.get(), data, size);
    }

    awaitable_shutdown shutdown()
    {
        return awaitable_shutdown(*_tcp_ptr.get());
    }

    ~tcp()
    {
        if (_tcp_ptr)
            uv_close((uv_handle_t*) _tcp_ptr.release(), on_close);
    }

private:
    static void on_close(uv_handle_t* handle)
    {
        assert(handle != nullptr);
        delete handle;
    }

private:
    tcp_uv_ptr _tcp_ptr;
};

class awaitable_connect
{
public:
    explicit awaitable_connect(const std::string& ip, uint16_t port)
        : _ip(ip)
        , _port(port)
        , _tcp_ptr(std::make_unique<uv_tcp_t>())
    {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::experimental::coroutine_handle<> awaiting_coroutine) noexcept
    {
        std::cout << "connect_suspend\n";
        _coro = awaiting_coroutine;
        uv_tcp_init(co::base::get_scheduler().uv_loop(), _tcp_ptr.get());
        _connect.data = static_cast<void*>(this);
        struct sockaddr_in dest;
        uv_ip4_addr(_ip.c_str(), _port, &dest);
        int ret = uv_tcp_connect(&_connect, _tcp_ptr.get(), (const struct sockaddr*)&dest, on_connect);
        std::cout << ret << "\n";
        if (ret != 0)
        {
            _status = ret;
            co::base::get_scheduler().ready(awaiting_coroutine);
        }
    }

    tcp await_resume()
    {
        std::cout << "connect_resume\n";
        using namespace std::string_literals;

        if (_status != 0)
            throw std::runtime_error("connection error: "s + uv_strerror(_status));

        return tcp{ std::move(_tcp_ptr) };
    }

private:
    static void on_connect(uv_connect_t* connect, int status)
    {
        std::cout << "on connect\n";
        assert(connect != nullptr);
        assert(connect->data != nullptr);
        auto& self = *static_cast<awaitable_connect*>(connect->data);
        self._status = status;
        co::base::get_scheduler().ready(self._coro);
    }

private:
    int _status = 0;
    std::experimental::coroutine_handle<> _coro;
    uv_connect_t _connect;
    tcp_uv_ptr _tcp_ptr;
    std::string _ip;
    uint16_t _port;
};


awaitable_connect connect(const std::string& ip, uint16_t port)
{
    return awaitable_connect(ip, port);
}

}