#pragma once

#include <co/check.hpp>
#include <co/status_code.hpp>

namespace co::impl
{

enum core_codes
{
    cancel = 1,
    timeout = 2,
    empty = 3,
    full = 4,
    closed = 5,
    broken = 6,
    other = 7
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
        case core_codes::empty:
            return "empty";
        case core_codes::full:
            return "full";
        case core_codes::closed:
            return "closed";
        case core_codes::other:
            return "other";
        }
        CO_DCHECK(false);
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
constexpr auto full = impl::make_status_code(impl::core_codes::full);
constexpr auto empty = impl::make_status_code(impl::core_codes::empty);
constexpr auto closed = impl::make_status_code(impl::core_codes::closed);
constexpr auto other = impl::make_status_code(impl::core_codes::other);

}  // namespace co