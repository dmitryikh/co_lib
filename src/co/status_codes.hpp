#pragma once

#include <cassert>
#include <co/status_code.hpp>

namespace co::impl
{

enum core_codes
{
    cancel = 1,
    timeout = 2,
    broken = 3,
    other = 4
};

class core_codes_category : public status_category
{
    constexpr static uint64_t id = 0x409f1f7642851de6;

public:
    core_codes_category()
        : co::status_category(id)
    {}

    [[nodiscard]] const char* name() const noexcept override
    {
        return "co_lib";
    }

    [[nodiscard]] const char* message(int ev) const noexcept override
    {
        switch (static_cast<core_codes>(ev))
        {
        case core_codes::cancel:
            return "cancel";
        case core_codes::timeout:
            return "timeout";
        case core_codes::broken:
            return "broken";
        case core_codes::other:
            return "other";
        }
        assert(false);
        return "undefined";
    }
};

const core_codes_category global_core_codes_category{};

inline constexpr co::status_code make_status_code(core_codes e)
{
    return co::status_code{ e, &global_core_codes_category };
}

}  // namespace co::impl

namespace co
{

constexpr auto cancel = impl::make_status_code(impl::core_codes::cancel);
constexpr auto timeout = impl::make_status_code(impl::core_codes::timeout);
constexpr auto broken = impl::make_status_code(impl::core_codes::broken);
constexpr auto other = impl::make_status_code(impl::core_codes::other);

}  // namespace co