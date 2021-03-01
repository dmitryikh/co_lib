#pragma once

#include <uv.h>

#include <co/func.hpp>
#include <co/result.hpp>
#include <co/until.hpp>

#include <co/net/impl/uv_tcp_ptr.hpp>
#include <co/net/tcp_stream.hpp>

namespace co::net
{

namespace impl
{
struct tcp_listener_shared_state;
}

class tcp_stream;

/// \brief listens for incoming tcp connection
///
/// Usage:
/// \code
///     auto listener = co_await co::net::tcp_listener::bind(ip, port).unwrap();
///
///     while (true)
///     {
///         co::net::tcp_stream tcp_stream = co_await listener.accept().unwrap();
///         co::thread(serve_client(std::move(tcp_stream))).detach();
///     }
/// \endcode
class tcp_listener
{
    friend class tcp_stream;
    static constexpr int INCOMING_MAX = 128;

private:
    /// \brief create tcp_listener from the state object
    explicit tcp_listener(std::unique_ptr<impl::tcp_listener_shared_state>&& state);

public:
    tcp_listener(const tcp_listener&) = delete;
    tcp_listener& operator=(const tcp_listener&) = delete;

    tcp_listener(tcp_listener&& other);
    tcp_listener& operator=(tcp_listener&& other);

    ~tcp_listener();

    /// \brief bind ip:port to be ready to accept incoming tcp connections
    /// \param ip in format "127.0.0.1"
    /// \param port
    /// \return tcp_listener or network error
    static co::func<co::result<tcp_listener>> bind(const std::string& ip, uint16_t port);

    /// \brief waits for an incoming connection
    /// \return co::net::tcp_stream or interruption status or network error
    co::func<co::result<tcp_stream>> accept(co::until until = {});

private:
    static void on_new_connection(uv_stream_t* server, int status);

private:
    std::unique_ptr<impl::tcp_listener_shared_state> _state;
};

}  // namespace co::net
