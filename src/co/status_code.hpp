#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <co/check.hpp>

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
    constexpr status_code(T code)  // NOLINT(google-explicit-constructor)
        : status_code(make_status_code(code))
    {}

    template <StatusEnum T>
    constexpr status_code(T code, const status_category* category)
        : _category(category)
        , _code(static_cast<int>(code))
    {
        CO_DCHECK(category != nullptr);
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
        return !operator==(lhs, code);
    }

    template <StatusEnum T>
    friend bool operator!=(const T& code, const status_code& rhs)
    {
        return !operator==(rhs, code);
    }

    [[nodiscard]] const char* category_name() const
    {
        return _category->name();
    }

    [[nodiscard]] const char* message() const
    {
        return _category->message(_code);
    }

    [[nodiscard]] int code() const
    {
        return _code;
    }

private:
    const status_category* _category;
    int _code;
};

}  // namespace co