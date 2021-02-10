#pragma once

#include <exception>
#include <system_error>

namespace co
{

class exception;

class error_desc : public std::error_code
{
public:
    error_desc() = default;  // boost::outcome::result needs that

    error_desc(const std::error_code& errc, const char* desc)
        : std::error_code(errc)
        , _desc(desc)
    {}

    error_desc(const std::error_code& errc)
        : std::error_code(errc)
    {}

    error_desc(const co::exception& coexc);

    const char* what() const
    {
        assert(_desc != nullptr);
        return _desc;
    }

    const std::error_code& errc() const
    {
        return *this;
    }

private:
    const char* _desc = "";  // error descriprion, MUST HAVE static lifetime duration
};

class exception : public std::exception
{
public:
    exception(const std::error_code& errc, const char* desc = "")
        : _edesc(errc, desc)
    {}

    explicit exception(const error_desc& edesc)
        : _edesc(edesc)
    {}

    const char* what() const noexcept override
    {
        return _edesc.what();
    }

    const std::error_code& errc() const noexcept
    {
        return _edesc.errc();
    }

    const error_desc& err() const noexcept
    {
        return _edesc;
    }

private:
    error_desc _edesc;
};

inline std::ostream& operator<<(std::ostream& out, const co::exception& coexc)
{
    out << coexc.errc().category().name() << "::" << coexc.errc().message() << "(" << coexc.errc().value() << ") "
        << coexc.what();
    return out;
}

inline error_desc::error_desc(const co::exception& coexc)
    : error_desc(coexc.errc(), coexc.what())
{}

}  // namespace co