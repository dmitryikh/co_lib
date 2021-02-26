#pragma once

#include <cassert>
#include <concepts>
#include <type_traits>

namespace co
{

class status_category
{
public:
    explicit status_category(uint64_t id)
        : _id(id)
    {}

    [[nodiscard]] virtual const char* name() const noexcept = 0;
    [[nodiscard]] virtual const char* message(int status) const noexcept = 0;

    [[nodiscard]] uint64_t id() const
    {
        return _id;
    }

private:
    uint64_t _id;
};

class status_code;

template <typename T>
concept StatusCodeConcept = std::is_convertible_v<T, status_code>;

// clang-format off
template <typename T>
concept StatusEnum = requires(T e)
{
    { make_status_code(e) } -> StatusCodeConcept;
};
// clang-format on

class status_code
{
public:
    template <StatusEnum T>
    status_code(T code)
        : status_code(make_status_code(code))
    {}

    status_code(const status_code& status_code)
        : _code(status_code._code)
        , _category(status_code._category)
    {}

    template <StatusEnum T>
    status_code(T code, const status_category* category)
        : _category(category)
        , _code(static_cast<int>(code))
    {
        assert(category != nullptr);
    }

    status_code& operator=(const status_code&) = default;

    friend bool operator==(const status_code& lhs, const status_code& rhs)
    {
        return (lhs._code == rhs._code) && (lhs._category->id() == rhs._category->id());
    }

    friend bool operator!=(const status_code& lhs, const status_code& rhs)
    {
        return !(lhs == rhs);
    }

    template <StatusEnum T>
    friend bool operator==(const status_code& lhs, const T& code)
    {
        return lhs == make_status_code(code);
    }

    template <StatusEnum T>
    friend bool operator==(const T& code, const status_code& rhs)
    {
        return rhs == code;
    }

    template <StatusEnum T>
    friend bool operator!=(const status_code& lhs, const T& code)
    {
        return !(lhs == code);
    }

    template <StatusEnum T>
    friend bool operator!=(const T& code, const status_code& rhs)
    {
        return !(rhs == code);
    }

    [[nodiscard]] const char* category_name() const
    {
        return _category->name();
    }

    [[nodiscard]] const char* message() const
    {
        return _category->message(_code);
    }

private:
    const status_category* _category;
    int _code;
};

namespace impl
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

inline co::status_code make_status_code(core_codes e)
{
    const static core_codes_category global_core_codes_category;
    return co::status_code{ e, &global_core_codes_category };
}

}  // namespace impl

constexpr auto cancel = impl::core_codes::cancel;
constexpr auto timeout = impl::core_codes::timeout;
constexpr auto broken = impl::core_codes::broken;
constexpr auto other = impl::core_codes::other;

}  // namespace co