#pragma once

#include <iostream>

#include <co/net/status_codes.hpp>
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

class address
{
    friend class tcp_stream;

private:
    address(std::string&& ip, uint16_t port, address_family family)
        : _ip(std::move(ip))
        , _port(port)
        , _family(family)
    {}

    static address from(const sockaddr_storage& addr_storage)
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

inline std::ostream& operator<<(std::ostream& out, const address& address)
{
    out << address.ip() << ":" << address.port();
    return out;
}

}  // namespace co::net
