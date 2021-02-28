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

std::ostream& operator<<(std::ostream& out, const exception& coexc)
{
    out << coexc.status().category_name() << "::" << coexc.status().message() << "(" << coexc.status().message() << ") "
        << coexc.what();
    return out;
}

}  // namespace co