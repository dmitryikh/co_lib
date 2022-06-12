#include <co/net/tcp_stream.hpp>

#include <array>
#include <co/event.hpp>
#include <co/impl/scheduler.hpp>
#include <co/net/status_codes.hpp>

namespace co::net
{

co::func<co::result<tcp_stream>> tcp_stream::connect(const std::string& ip, uint16_t port)
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
      CO_DCHECK(connect != nullptr);
      CO_DCHECK(connect->data != nullptr);

      auto& state = *static_cast<connect_state*>(connect->data);
      state.status = status;
      state.ev.notify();
    };

    sockaddr_in dest{};
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

co::func<co::result<std::span<char>>> tcp_stream::read(std::span<char> buffer)
{
    struct read_state
    {
        std::span<char> buffer;
        int read_status;
        size_t read_len;
        event& ev;
    };

    event ev;

    auto state = read_state{ buffer, 0, 0, ev };

    auto alloc = [](uv_handle_t* handle, size_t /*suggested_size*/, uv_buf_t* buf)
    {
      CO_DCHECK(handle != nullptr);
      CO_DCHECK(handle->data != nullptr);

      auto& state = *static_cast<read_state*>(handle->data);
      buf->base = state.buffer.data();
      buf->len = static_cast<int>(state.buffer.size());
    };

    auto on_read = [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* /*buf*/)
    {
      CO_DCHECK(stream != nullptr);
      CO_DCHECK(stream->data != nullptr);

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

    CO_CHECK(_tcp_ptr);
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

    CO_DCHECK(state.read_len <= buffer.size());
    co_return co::ok(buffer.subspan(0, state.read_len));
}

co::func<co::result<void>> tcp_stream::write(std::span<const char> buffer)
{
    struct write_state
    {
        int status;
        event& ev;
    };

    event ev;
    std::array<uv_buf_t, 1> bufs = { uv_buf_init(const_cast<char*>(buffer.data()),
                                                 static_cast<int>(buffer.size())) };

    auto state = write_state{ 0, ev };

    auto on_write = [](uv_write_t* handle, int status)
    {
      CO_DCHECK(handle != nullptr);
      CO_DCHECK(handle->data != nullptr);

      auto& state = *static_cast<write_state*>(handle->data);
      state.status = status;
      state.ev.notify();
    };

    uv_write_t write_handle;
    write_handle.data = static_cast<void*>(&state);
    CO_CHECK(_tcp_ptr);
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

co::func<co::result<void>> tcp_stream::shutdown()
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
      CO_DCHECK(handle != nullptr);
      CO_DCHECK(handle->data != nullptr);

      auto& state = *static_cast<shutdown_state*>(handle->data);
      state.status = status;
      state.ev.notify();
    };

    uv_shutdown_t shutdown_handle;
    shutdown_handle.data = static_cast<void*>(&state);
    CO_CHECK(_tcp_ptr);
    const int ret = uv_shutdown(&shutdown_handle, (uv_stream_t*)_tcp_ptr.get(), on_shutdown);
    using namespace std::string_literals;
    if (ret != 0)
        co_return co::err(other_net);

    co_await ev.wait();

    if (state.status != 0)
        co_return co::err(other_net);

    co_return co::ok();
}

address tcp_stream::local_address() const
{
    sockaddr_storage addr_storage{};
    int len = sizeof(sockaddr_storage);
    auto res = uv_tcp_getsockname(_tcp_ptr.get(), (struct sockaddr*)&addr_storage, &len);
    if (res != 0)
        throw co::exception(other_net, "uv_tcp_getsockname failed");

    return address::from(addr_storage);
}

address tcp_stream::peer_address() const
{
    sockaddr_storage addr_storage{};
    int len = sizeof(sockaddr_storage);
    auto res = uv_tcp_getpeername(_tcp_ptr.get(), (struct sockaddr*)&addr_storage, &len);
    if (res != 0)
        throw co::exception(other_net, "uv_tcp_getpeername failed");

    return address::from(addr_storage);
}

}
