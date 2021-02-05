#pragma once

#include <boost/outcome.hpp>
#include <co/exception.hpp>

namespace co
{

namespace impl
{

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

}  // namespace impl

template <typename T>
class result
{
private:
    using result_type = impl::outcome::result<T, error_desc>;

public:
    using ok_type = T;
    using err_type = error_desc;

public:
    /// construct errors
    result(error_desc&& errc)
        : _res(impl::outcome::failure(std::move(errc)))
    {}

    result(const error_desc& errc)
        : _res(impl::outcome::failure(errc))
    {}

    result(const std::error_code& errc)
        : _res(impl::outcome::failure(errc))
    {}

    /// construct successful result
    result(impl::outcome::success_type<void>&& success) requires (std::is_same_v<T, void>)
        : _res(std::move(success))
    {}

    template <typename Arg>
    result(impl::outcome::success_type<Arg>&& success) requires (std::is_constructible_v<T, Arg>)
        : _res(std::move(success))
    {}

    bool is_err() const noexcept
    {
        return _res.has_error();
    }

    const std::error_code& errc() const noexcept
    {
        return _res.error().errc;
    }

    const error_desc& err() const noexcept
    {
        return _res.error();
    }

    const char* what() const noexcept
    {
        return _res.error().what();
    }

    bool is_ok() const noexcept
    {
        return _res.has_value();
    }

    // auto value() noexcept requires (!std::is_same_v<T, void>) 
    // {
    //     return _res.as_value();
    // }

    // auto value() const noexcept requires (!std::is_same_v<T, void>)
    // {
    //     return _res.as_value();
    // }

    void unwrap() const& noexcept(false) requires std::is_same_v<T, void> 
    {
        if (is_err())
            throw co::exception(_res.assume_error());
    }

    std::add_lvalue_reference_t<T> unwrap() & noexcept(false) requires (!std::is_same_v<T, void>)
    {
        if (is_err())
            throw co::exception(_res.assume_error());
        return _res.assume_value();
    }

    std::add_lvalue_reference_t<const T> unwrap() const& noexcept(false) requires (!std::is_same_v<T, void>)
    {
        if (is_err())
            throw co::exception(_res.assume_error());
        return _res.assume_value();
    }

    std::add_rvalue_reference_t<T> unwrap() && noexcept(false) requires (!std::is_same_v<T, void>)
    {
        if (is_err())
            throw co::exception(_res.assume_error());
        return std::move(_res.assume_value());
    }

private:
    result_type _res;
};

template <typename T>
bool operator== (const result<T>& r, const std::error_code& errc)
{
    if (!r.is_err())
        return false;

    return r.err() == errc;
}

template <typename T>
bool operator== (const std::error_code& errc, const result<T>& r)
{
    return r == errc;
}

template <typename T>
bool operator!= (const result<T>& r, const std::error_code& errc)
{
    return !(r == errc);
}

template <typename T>
bool operator!= (const std::error_code& errc, const result<T>& r)
{
    return !(r == errc);
}

template <typename T>
std::ostream& operator<< (std::ostream& out, const result<T>& r)
{
    if (r.is_err())
    {
        out << "ERR " << r.err().category().name() << "::" << r.err().message() << "(" << r.err().value() << ") " << r.what();
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

template <typename... Args>
error_desc err(Args&&... args)
{
    return { std::forward<Args>(args)... };
}

auto ok()
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
struct is_result : std::false_type {};

template <typename T>
struct is_result<result<T>> : std::true_type {};

}  // namespace impl

template <typename T>
inline constexpr bool is_result_v = impl::is_result<T>::value;

}  // namespace co