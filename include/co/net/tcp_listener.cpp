#include <co/net/tcp_listener.hpp>

#include <cassert>
#include <queue>
#include <variant>
#include <co/condition_variable.hpp>
#include <co/net/status_codes.hpp>

namespace co::net::impl
{

struct connection_msg
{};
struct error_msg
{
    int status;
};
using message = std::variant<connection_msg, error_msg>;

// shared state is needed because we pass the (void*) address on it into libuv
// request, we need that the address be changed with moving
struct tcp_listener_shared_state
{
    explicit tcp_listener_shared_state(uv_tcp_ptr&& tcp_ptr)
        : _server_tcp_ptr(std::move(tcp_ptr))
    {}

    bool _stopping = false;
    impl::uv_tcp_ptr _server_tcp_ptr;
    std::queue<message> _queue;
    co::condition_variable _cv;
};

}  // namespace co::net::impl

namespace co::net
{

tcp_listener::tcp_listener(std::unique_ptr<impl::tcp_listener_shared_state>&& state)
    : _state(std::move(state))
{
    assert(_state != nullptr);
    assert(_state->_server_tcp_ptr != nullptr);
}

tcp_listener::tcp_listener(tcp_listener&& other) = default;
tcp_listener& tcp_listener::operator=(tcp_listener&& other) = default;
tcp_listener::~tcp_listener() = default;

co::func<co::result<tcp_listener>> tcp_listener::bind(const std::string& ip, uint16_t port)
{

    sockaddr_in addr{};
    int ret = uv_ip4_addr(ip.c_str(), port, &addr);
    if (ret != 0)
        co_return co::err(wrong_address);

    auto server_socket = impl::make_and_init_uv_tcp_handle();
    auto state = std::make_unique<impl::tcp_listener_shared_state>(std::move(server_socket));

    ret = uv_tcp_bind(state->_server_tcp_ptr.get(), (const struct sockaddr*)&addr, 0);
    if (ret != 0)
        co_return co::err(other_net, uv_strerror(ret));

    state->_server_tcp_ptr->data = static_cast<void*>(state.get());
    ret = uv_listen((uv_stream_t*)state->_server_tcp_ptr.get(), INCOMING_MAX, tcp_listener::on_new_connection);
    if (ret != 0)
        co_return co::err(other_net, uv_strerror(ret));

    co_return co::ok(tcp_listener{ std::move(state) });
}

void tcp_listener::on_new_connection(uv_stream_t* server, int status)
{
    assert(server != nullptr);
    assert(server->data != nullptr);

    auto& state = *static_cast<impl::tcp_listener_shared_state*>(server->data);

    if (status < 0)
        state._queue.push(impl::error_msg{ status });
    else
        state._queue.push(impl::connection_msg{});
    state._cv.notify_one();
}

co::func<co::result<tcp_stream>> tcp_listener::accept(co::until until)
{
    while (_state->_queue.empty() && !_state->_stopping)
    {
        auto res = co_await _state->_cv.wait(until);
        if (res.is_err())
            co_return res.err();
    }

    if (_state->_stopping)
    {
        // TODO: use co::closed here after moving co::channel error codes to the common place
        co_return co::err(co::other);
    }

    assert(!_state->_queue.empty());
    impl::message msg = _state->_queue.front();
    _state->_queue.pop();

    if (std::holds_alternative<impl::connection_msg>(msg))
    {
        auto client_tcp_ptr = impl::make_and_init_uv_tcp_handle();

        auto res = uv_accept((uv_stream_t*)_state->_server_tcp_ptr.get(), (uv_stream_t*)client_tcp_ptr.get());
        if (res != 0)
            co_return co::err(other_net, uv_strerror(res));

        co_return co::ok(tcp_stream{ std::move(client_tcp_ptr) });
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

}  // namespace co::net