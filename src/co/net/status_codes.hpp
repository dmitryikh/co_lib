#pragma once

#include <co/status_code.hpp>

namespace co::net::impl
{

enum class net_code
{
    eof = 1,
    wrong_address = 2,
    other_net = 3
};

class net_code_category : public status_category
{
    constexpr static uint64_t id = 0xf86aa57188f959fd;

public:
    net_code_category()
        : co::status_category(id)
    {}

    [[nodiscard]] const char* name() const noexcept override
    {
        return "co_net";
    }

    [[nodiscard]] const char* message(int ev) const noexcept override
    {
        switch (static_cast<net_code>(ev))
        {
        case net_code::eof:
            return "eof";
        case net_code::wrong_address:
            return "wrong address";
        case net_code::other_net:
            return "other net";
        }
        CO_DCHECK(false);
        return "undefined";
    }
};

const net_code_category global_net_code_category{};

inline constexpr co::status_code make_status_code(net_code e)
{
    return co::status_code{ e, &global_net_code_category };
}

}  // namespace co::net::impl

namespace co::net
{

constexpr auto eof = impl::make_status_code(impl::net_code::eof);
constexpr auto wrong_address = impl::make_status_code(impl::net_code::wrong_address);
constexpr auto other_net = impl::make_status_code(impl::net_code::other_net);

}  // namespace co::net