#pragma once

#include <iosfwd>
#include <string>
#include <uv.h>

namespace co::net
{

enum class address_family
{
    ip4 = 1,
    ip6 = 2
};

constexpr address_family ip4 = address_family::ip4;
constexpr address_family ip6 = address_family::ip6;

class tcp_stream;

/// \brief holds network address information
///
/// Usage:
/// \code
///     co::net::tcp_stream stream = ...;
///     std::cout << "local address: " << stream.local_address() << "\n";
///     std::cout << "peer address: " << stream.peer_address() << "\n";
/// \endcode
class address
{
    friend class tcp_stream;

private:
    address(std::string&& ip, uint16_t port, address_family family)
        : _ip(std::move(ip))
        , _port(port)
        , _family(family)
    {}

    static address from(const sockaddr_storage& addr_storage);

public:
    [[nodiscard]] address_family family() const
    {
        return _family;
    }

    [[nodiscard]] const std::string& ip() const
    {
        return _ip;
    }

    [[nodiscard]] uint16_t port() const
    {
        return _port;
    }

private:
    const std::string _ip;
    const uint16_t _port;
    const address_family _family;
};

std::ostream& operator<<(std::ostream& out, const address& address);

}  // namespace co::net
