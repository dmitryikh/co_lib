#pragma once

#include <exception>
#include <system_error>

namespace co
{

namespace impl
{

class error_code_desc;

}  // namespace impl

class exception : public std::exception
{
    friend class impl::error_code_desc;
public:
    exception(const std::error_code& errc, const char* desc = "")
        : _errc(errc)
        , _desc(desc)
    {
    }

    const char* what() const noexcept override
    {
        assert(_desc != nullptr);
        return _desc;
    }

    std::error_code errc() const noexcept
    {
        return _errc;
    }

private:
    std::error_code _errc{};
    const char* _desc = "";
};

std::ostream& operator<< (std::ostream& out, const co::exception& coexc)
{
    out << coexc.errc().category().name() << "::" << coexc.errc().message() << "(" << coexc.errc().value() << ") " << coexc.what();
    return out;
}

}  // namespace co