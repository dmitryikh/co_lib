#pragma once

#include <boost/outcome.hpp>
#include <co/exception.hpp>

namespace co
{

namespace impl
{

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

class error_code_desc : public std::error_code
{
public:
    error_code_desc() = default;  // outcome::result needs that

    error_code_desc(const std::error_code& errc, const char* desc)
        : errc(errc)
        , desc(desc)
    {}

    error_code_desc(const std::error_code& errc)
        : errc(errc)
    {}

    error_code_desc(const co::exception& coexc)
        : errc(coexc._errc)
        , desc(coexc._desc)
    {}

public:
    std::error_code errc{};
    const char* desc = "";  // error descriprion
};

}  // namespace impl

template <typename T>
class result
{
private:
    using result_type = impl::outcome::result<T, impl::error_code_desc>;

public:
    result(impl::error_code_desc&& errc)
        : _res(impl::outcome::failure(std::move(errc)))
    {}

    template <typename Arg>
    result(Arg&& arg)
        : _res(std::forward<Arg>(arg))
    {}

    bool is_err() const noexcept
    {
        return _res.has_error();
    }

    const std::error_code& err() const noexcept
    {
        return _res.assume_error().errc;
    }

    const char* what() const noexcept
    {
        return _res.assume_error().desc;
    }

    bool is_ok() const noexcept
    {
        return _res.has_value();
    }

    auto value() noexcept requires (!std::is_same_v<T, void>) 
    {
        return _res.assume_value();
    }

    auto value() const noexcept requires (!std::is_same_v<T, void>)
    {
        return _res.assume_value();
    }

    void unwrap() const& noexcept(false) requires std::is_same_v<T, void> 
    {
        if (is_err())
            throw co::exception(_res.assume_error().errc, _res.assume_error().desc);
    }

    std::add_lvalue_reference_t<T> unwrap() & noexcept(false) requires (!std::is_same_v<T, void>)
    {
        if (is_err())
            throw co::exception(_res.assume_error().errc, _res.assume_error().desc);
        return _res.assume_value();
    }

    std::add_lvalue_reference_t<const T> unwrap() const& noexcept(false) requires (!std::is_same_v<T, void>)
    {
        if (is_err())
            throw co::exception(_res.assume_error().errc, _res.assume_error().desc);
        return _res.assume_value();
    }

    std::add_rvalue_reference_t<T> unwrap() && noexcept(false) requires (!std::is_same_v<T, void>)
    {
        if (is_err())
            throw co::exception(_res.assume_error().errc, _res.assume_error().desc);
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
impl::error_code_desc err(Args&&... args)
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

}  // namespace co