#pragma once

#include <exception>
#include <iosfwd>
#include <co/status_codes.hpp>

namespace co
{

class exception;

class error_desc : public co::status_code
{
public:
    error_desc(const co::status_code& status, const char* desc = "");

    error_desc(const co::exception& coexc);

    [[nodiscard]] const char* what() const
    {
        CO_DCHECK(_desc != nullptr);
        return _desc;
    }

    [[nodiscard]] const co::status_code& status() const
    {
        return *this;
    }

private:
    const char* _desc = "";  // error description, MUST HAVE static lifetime duration
};

/// \brief co_lib exception class that holds co::status_code and custom description as not owned const char*
///
/// NOTE: const char* desc must have static lifetime
/// Usage:
/// \code
///     throw co::exception(co::other, "something bad happened");
/// \endcode
/// How to choose between co::result and co::exception? General rule is the next: use return code (co::result) for
/// expected outcomes of the operation: network errors, operation cancellation, channel is full. And use co::exception
/// (or more general std::exception) as a not expected error. like class misuse, unexpected errors of underlying
/// systems, allocation errors, logic errors, etc.
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

std::ostream& operator<<(std::ostream& out, const co::error_desc& desc);
std::ostream& operator<<(std::ostream& out, const co::exception& coexc);

}  // namespace co