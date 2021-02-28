#pragma once

#include <exception>
#include <iostream>
#include <co/status_codes.hpp>

namespace co
{

class exception;

class error_desc : public co::status_code
{
public:
    // TODO: boost::outcome::result needs that
    error_desc()
        : co::status_code(co::other)
    {}

    error_desc(const co::status_code& status, const char* desc)
        : co::status_code(status)
        , _desc(desc)
    {}

    error_desc(const co::status_code& status)
        : co::status_code(status)
    {}

    error_desc(const co::exception& coexc);

    [[nodiscard]] const char* what() const
    {
        assert(_desc != nullptr);
        return _desc;
    }

    [[nodiscard]] const co::status_code& status() const
    {
        return *this;
    }

private:
    const char* _desc = "";  // error description, MUST HAVE static lifetime duration
};

class exception : public std::exception
{
public:
    exception(const co::status_code& status, const char* desc = "")
        : _edesc(status, desc)
    {}

    explicit exception(const error_desc& edesc)
        : _edesc(edesc)
    {}

    [[nodiscard]] const char* what() const noexcept override
    {
        return _edesc.what();
    }

    [[nodiscard]] const co::status_code& status() const noexcept
    {
        return _edesc.status();
    }

    [[nodiscard]] const error_desc& err() const noexcept
    {
        return _edesc;
    }

private:
    error_desc _edesc;
};

inline std::ostream& operator<<(std::ostream& out, const co::exception& coexc)
{
    out << coexc.status().category_name() << "::" << coexc.status().message() << "(" << coexc.status().message() << ") "
        << coexc.what();
    return out;
}

inline error_desc::error_desc(const co::exception& coexc)
    : error_desc(coexc.status(), coexc.what())
{}

}  // namespace co