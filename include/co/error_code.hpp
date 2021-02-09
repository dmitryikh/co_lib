#pragma once

#include <system_error>

namespace co::impl
{

enum class error_code
{
    cancel = 1,
    timeout = 2,
    broken = 3,
    other = 4
};

struct error_code_category : std::error_category
{
    const char* name() const noexcept override
    {
        return "co_lib";
    }

    std::string message(int ev) const override
    {
        switch (static_cast<error_code>(ev))
        {
        case error_code::cancel:
            return "cancel";
        case error_code::timeout:
            return "timeout";
        case error_code::broken:
            return "broken";
        case error_code::other:
            return "other";
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

}  // namespace co::impl

namespace std {
    template <> struct is_error_code_enum<co::impl::error_code> : true_type {};
}

namespace co
{

using error_code = impl::error_code;

constexpr auto cancel = error_code::cancel;
constexpr auto timeout = error_code::timeout;
constexpr auto broken = error_code::broken;
constexpr auto other = error_code::other;

}  // namespace co