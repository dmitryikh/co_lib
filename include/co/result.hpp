#pragma once

#include <boost/outcome.hpp>
#include <co/exception.hpp>

namespace co
{

namespace impl
{

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

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
    using result_type = impl::outcome::result<T, error_desc>;

public:
    using ok_type = T;
    using err_type = error_desc;

public:
    /// \brief build an error result from the error description object
    result(error_desc errc) // NOLINT(google-explicit-constructor)
        : _res(impl::outcome::failure(errc))
    {}

    /// \brief build an error result from the error code
    result(const std::error_code& errc) // NOLINT(google-explicit-constructor)
        : _res(impl::outcome::failure(errc))
    {}

    /// \brief build an successful result from void (for T = void)
    result(impl::outcome::success_type<void>&& success) requires(std::is_same_v<T, void>) // NOLINT(google-explicit-constructor)
        : _res(std::move(success))
    {}

    /// \brief build an successful result from any Arg. Construct success type inplace
    template <typename Arg>
    result(impl::outcome::success_type<Arg>&& success) requires(std::is_constructible_v<T, Arg>) // NOLINT(google-explicit-constructor)
        : _res(std::move(success))
    {}

    /// \brief returns true if the result contains an error description
    [[nodiscard]] bool is_err() const noexcept
    {
        return _res.has_error();
    }

    /// \brief returns an error code if the result contains an error. Throws an exception otherwise
    [[nodiscard]] const std::error_code& errc() const
    {
        return _res.error().errc();
    }

    /// \brief returns an error description if the result contains an error. Throws an exception otherwise
    [[nodiscard]] const error_desc& err() const
    {
        return _res.error();
    }

    /// \brief returns a textual part of the error description if the result contains an error. Throws an exception
    /// otherwise
    [[nodiscard]] const char* what() const noexcept
    {
        return _res.error().what();
    }

    /// \brief returns true if the result contains a successful type
    [[nodiscard]] bool is_ok() const noexcept
    {
        return _res.has_value();
    }

    /// \brief will throw an error result as exception. Do nothing otherwise
    void unwrap() const& noexcept(false) requires std::is_same_v<T, void>
    {
        if (is_err())
            throw co::exception(_res.assume_error());
    }

    /// \brief get a reference to a successful type of the result. Throws an error as exception if the result contains
    /// an error case
    std::add_lvalue_reference_t<T> unwrap() & noexcept(false) requires(!std::is_same_v<T, void>)
    {
        if (is_err())
            throw co::exception(_res.assume_error());
        return _res.assume_value();
    }

    /// \brief get a const reference to a successful type of the result. Throws an error as exception if the result
    /// contains an error case
    std::add_lvalue_reference_t<const T> unwrap() const& noexcept(false) requires(!std::is_same_v<T, void>)
    {
        if (is_err())
            throw co::exception(_res.assume_error());
        return _res.assume_value();
    }

    /// \brief get a rvalue to a successful type of the result. Throws an error as exception if the result
    /// contains an error case
    std::add_rvalue_reference_t<T> unwrap() && noexcept(false) requires(!std::is_same_v<T, void>)
    {
        if (is_err())
            throw co::exception(_res.assume_error());
        return std::move(_res.assume_value());
    }

private:
    result_type _res;
};

template <typename T>
bool operator==(const result<T>& r, const std::error_code& errc)
{
    if (!r.is_err())
        return false;

    return r.err() == errc;
}

template <typename T>
bool operator==(const std::error_code& errc, const result<T>& r)
{
    return r == errc;
}

template <typename T>
bool operator!=(const result<T>& r, const std::error_code& errc)
{
    return !(r == errc);
}

template <typename T>
bool operator!=(const std::error_code& errc, const result<T>& r)
{
    return !(r == errc);
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const result<T>& r)
{
    if (r.is_err())
    {
        out << "ERR " << r.err().category().name() << "::" << r.err().message() << "(" << r.err().value() << ") "
            << r.what();
    }
    else
    {
        if constexpr (std::is_same_v<T, void>)
        {
            out << "OK";
        }
        else
        {
            out << "OK " << r.unwrap();
        }
    }
    return out;
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
    return impl::outcome::success();
}

template <typename T>
auto ok(T&& t)
{
    return impl::outcome::success(std::forward<T>(t));
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