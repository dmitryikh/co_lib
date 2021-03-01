#pragma once

#include <variant>
#include <co/exception.hpp>
#include <co/status_code.hpp>

namespace co
{

namespace impl
{

template <typename T>
struct success_type
{
    explicit success_type(T&& value)
        : value(std::forward<T>(value))
    {}

    T value;
};

template <>
struct success_type<void>
{};

}  // namespace impl

/// \brief result represents either a successful value of type T or an error description
///
/// \code
///     co::result<int> res = co::ok(3);
///     assert(res.is_ok());
///     int i = res.unwrap();
///
///     res = co::err(co::timeout);
///     assert(res.is_err());
///     assert(res == co::timeout);  // sugar on (res.is_err() && res.err == co::timeout)
/// \endcode
///
template <typename T>
class [[nodiscard]] result
{
private:
    using variant = std::variant<T, error_desc>;

public:
    using ok_type = T;
    using err_type = error_desc;

public:
    /// \brief build an error result from the error description object
    result(error_desc status)  // NOLINT(google-explicit-constructor)
        : _res(std::in_place_index<1>, status)
    {}

    /// \brief build an error result from the error code
    result(const co::status_code& status)  // NOLINT(google-explicit-constructor)
        : _res(std::in_place_index<1>, status)
    {}

    /// \brief build an successful result from any Arg. Construct success type inplace
    template <typename Arg>
    result(impl::success_type<Arg>&& success) requires(  // NOLINT(google-explicit-constructor)
        std::is_constructible_v<T, Arg>)
        : _res(std::in_place_index<0>, std::move(success.value))
    {}

    /// \brief returns true if the result contains an error description
    [[nodiscard]] bool is_err() const noexcept
    {
        return _res.index() == 1;
    }

    /// \brief returns an error code if the result contains an error. Throws an exception otherwise
    [[nodiscard]] const co::status_code& status() const
    {
        return std::get<1>(_res).status();
    }

    /// \brief returns an error description if the result contains an error. Throws an exception otherwise
    [[nodiscard]] const error_desc& err() const
    {
        return std::get<1>(_res);
    }

    /// \brief returns a textual part of the error description if the result contains an error. Throws an exception
    /// otherwise
    [[nodiscard]] const char* what() const noexcept
    {
        return std::get<1>(_res).what();
    }

    /// \brief returns true if the result contains a successful type
    [[nodiscard]] bool is_ok() const noexcept
    {
        return _res.index() == 0;
    }

    /// \brief get a reference to a successful type of the result. Throws an error as exception if the result contains
    /// an error case
    std::add_lvalue_reference_t<T> unwrap() & noexcept(false)
    {
        if (is_err())
            throw co::exception(err());
        return std::get<0>(_res);
    }

    /// \brief get a const reference to a successful type of the result. Throws an error as exception if the result
    /// contains an error case
    std::add_lvalue_reference_t<const T> unwrap() const& noexcept(false)
    {
        if (is_err())
            throw co::exception(err());
        return std::get<0>(_res);
    }

    /// \brief get a rvalue to a successful type of the result. Throws an error as exception if the result
    /// contains an error case
    std::add_rvalue_reference_t<T> unwrap() && noexcept(false)
    {
        if (is_err())
            throw co::exception(err());
        return std::move(std::get<0>(_res));
    }

private:
    variant _res;
};

template <>
class result<void>
{
private:
    using variant = std::variant<std::monostate, error_desc>;

public:
    using ok_type = void;
    using err_type = error_desc;

public:
    result(error_desc status)  // NOLINT(google-explicit-constructor)
        : _res(status)
    {}

    result(const co::status_code& status)  // NOLINT(google-explicit-constructor)
        : _res(status)
    {}

    result(impl::success_type<void>&& success)  // NOLINT(google-explicit-constructor)
    {}

    [[nodiscard]] bool is_err() const noexcept
    {
        return _res.index() == 1;
    }

    [[nodiscard]] const co::status_code& status() const
    {
        return std::get<1>(_res).status();
    }

    [[nodiscard]] const error_desc& err() const
    {
        return std::get<1>(_res);
    }

    [[nodiscard]] const char* what() const noexcept
    {
        return std::get<1>(_res).what();
    }

    [[nodiscard]] bool is_ok() const noexcept
    {
        return _res.index() == 0;
    }

    void unwrap() const& noexcept(false)
    {
        if (is_err())
            throw co::exception(err());
    }

private:
    variant _res;
};

template <typename T>
bool operator==(const result<T>& r, const co::status_code& status)
{
    if (!r.is_err())
        return false;

    return r.err() == status;
}

template <typename T>
bool operator==(const co::status_code& status, const result<T>& r)
{
    return r == status;
}

template <typename T>
bool operator!=(const result<T>& r, const co::status_code& status)
{
    return !(r == status);
}

template <typename T>
bool operator!=(const co::status_code& status, const result<T>& r)
{
    return !(r == status);
}

inline error_desc err(const co::exception& coexc)
{
    return coexc.err();
}

template <typename... Args>
error_desc err(Args&&... args)
{
    return { std::forward<Args>(args)... };
}

inline auto ok()
{
    return impl::success_type<void>();
}

template <typename T>
auto ok(T&& t)
{
    return impl::success_type<T&&>(std::forward<T>(t));
}

namespace impl
{

template <typename T>
struct is_result : std::false_type
{};

template <typename T>
struct is_result<result<T>> : std::true_type
{};

}  // namespace impl

template <typename T>
inline constexpr bool is_result_v = impl::is_result<T>::value;

}  // namespace co