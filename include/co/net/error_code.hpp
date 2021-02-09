#pragma once

#include <boost/outcome.hpp>
#include <system_error>

namespace co::net::impl
{

enum class error_code
{
    eof = 1,
    wrong_address = 2,
    other_net = 3
};

struct error_code_category : std::error_category
{
    const char* name() const noexcept override
    {
        return "co_net";
    }

    std::string message(int ev) const override
    {
        switch (static_cast<error_code>(ev))
        {
        case error_code::eof:
            return "eof";
        case error_code::wrong_address:
            return "wrong address";
        case error_code::other_net:
            return "other net";
        }
        assert(false);
        return "undefined";
    }
};

const error_code_category global_error_code_category{};

inline std::error_code make_error_code(error_code e)
{
    return std::error_code{static_cast<int>(e), global_error_code_category};
}

}  // namespace co::net::impl

namespace std {
    template <> struct is_error_code_enum<co::net::impl::error_code> : true_type {};
}

namespace co::net
{

const auto eof = make_error_code(impl::error_code::eof);
const auto wrong_address = make_error_code(impl::error_code::wrong_address);
const auto other_net = make_error_code(impl::error_code::other_net);

}  // namespace co