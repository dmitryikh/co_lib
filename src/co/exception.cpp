#include <co/exception.hpp>

#include <iostream>

namespace co
{

error_desc::error_desc(const status_code& status, const char* desc)
    : co::status_code(status)
    , _desc(desc)
{}

error_desc::error_desc(const co::exception& coexc)
    : error_desc(coexc.status(), coexc.what())
{}

std::ostream& operator<<(std::ostream& out, const co::error_desc& desc)
{
    out << desc.status().category_name() << "::" << desc.status().message() << "(" << desc.status().code() << ") "
        << desc.what();
    return out;
}

std::ostream& operator<<(std::ostream& out, const co::exception& coexc)
{
    out << coexc.err();
    return out;
}

}  // namespace co