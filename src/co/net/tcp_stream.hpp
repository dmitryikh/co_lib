#pragma once

#include <cassert>
#include <span>

#include <uv.h>

#include <co/net/address.hpp>
#include <co/net/impl/uv_tcp_ptr.hpp>
#include <co/func.hpp>
#include <co/result.hpp>

namespace co::net
{

class tcp_listener;

/// \brief a full duplex tcp stream between two peers
///
/// tcp_stream can be obtained from client side by tcp_stream::connect(), or from server side by tcp_listener::accept
///
/// Usage:
/// \code
///     auto tcp_stream = co_await co::net::tcp_stream::connect(ip, port).unwrap();
///
///     std::string send_data = "hello world";
///     co_await tcp_stream.write(send_data).unwrap();
///
///     const size_t buffer_size = 100;
///     std::array<char, buffer_size> buffer;
///     std::span<char> received_data = co_await tcp_stream.read(buffer).unwrap();
///
///     co_await tcp_stream.shutdown().unwrap();
/// \endcode
class tcp_stream
{
    friend tcp_listener;

private:
    explicit tcp_stream(impl::uv_tcp_ptr tcp_ptr)
        : _tcp_ptr(std::move(tcp_ptr))
    {}

public:
    /// \brief connects to the remove ip:port
    /// \return tcp_stream or netrwork error
    static co::func<co::result<tcp_stream>> connect(const std::string& ip, uint16_t port);

    tcp_stream(const tcp_stream&) = delete;
    tcp_stream& operator=(const tcp_stream&) = delete;

    tcp_stream(tcp_stream&& other) = default;
    tcp_stream& operator=(tcp_stream&& other) = default;

    /// \brief reads from the tcp stream into buffer
    /// \return filled part of the buffer, or co::net::eof, or network error
    co::func<co::result<std::span<char>>> read(std::span<char> buffer);

    /// \brief writes from the buffer to the tcp stream
    /// \return network error in case of failure
    co::func<co::result<void>> write(std::span<const char> buffer);

    /// \brief perform graceful shutdown of the one side of tcp stream
    /// \return network error in case of failure
    co::func<co::result<void>> shutdown();

    /// \brief get a local address of the tcp stream
    [[nodiscard]] address local_address() const;

    /// \brief get a peer address of the tcp stream
    [[nodiscard]] address peer_address() const;

private:
    impl::uv_tcp_ptr _tcp_ptr;
};

}  // namespace co::net
