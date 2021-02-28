#include <co/net/address.hpp>

#include <array>
#include <iostream>

#include <co/exception.hpp>
#include <co/net/status_codes.hpp>

namespace co::net
{

address address::from(const sockaddr_storage& addr_storage)
{
    std::array<char, 32> buffer = { '\0' };

    if (addr_storage.ss_family == AF_INET)
    {
        sockaddr_in addr = (struct sockaddr_in&)addr_storage;
        auto res = uv_ip4_name(&addr, buffer.data(), buffer.size());
        if (res != 0)
            throw co::exception(other_net, "uv_ip4_name failed");

        auto port = static_cast<uint16_t>(ntohs(addr.sin_port));
        return { buffer.data(), port, ip4 };
    }
    else if (addr_storage.ss_family == AF_INET6)
    {
        sockaddr_in6 addr = (struct sockaddr_in6&)addr_storage;
        auto res = uv_ip6_name(&addr, buffer.data(), buffer.size());
        if (res != 0)
            throw co::exception(other_net, "uv_ip6_name failed");

        auto port = static_cast<uint16_t>(ntohs(addr.sin6_port));
        return { buffer.data(), port, ip6 };
    }
    else
    {
        throw co::exception(other_net, "unknown socket type");
    }
}

std::ostream& operator<<(std::ostream& out, const address& address)
{
    out << address.ip() << ":" << address.port();
    return out;
}

}  // namespace co::net