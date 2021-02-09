#pragma once

#include <boost/outcome.hpp>
#include <system_error>

namespace co::redis::impl
{

enum class error_code
{
    protocol_error = 1,
    unknown_data_type = 2,
    buffer_overflow = 3
};

struct error_code_category : std::error_category
{
    const char* name() const noexcept override
    {
        return "co_redis errors";
    }

    std::string message(int ev) const override
    {
        switch (static_cast<error_code>(ev))
        {
        case error_code::protocol_error:
            return "protocol erro";
        case error_code::unknown_data_type:
            return "unknown data type";
        case error_code::buffer_overflow:
            return "buffer overflow";
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

}  // namespace co::redis::impl

namespace std {
    template <> struct is_error_code_enum<co::redis::impl::error_code> : true_type {};
}

namespace co::redis
{

const auto protocol_error = make_error_code(impl::error_code::protocol_error);
const auto unknown_data_type = make_error_code(impl::error_code::unknown_data_type);
const auto buffer_overflow = make_error_code(impl::error_code::unknown_data_type);

}  // namespace co::redis