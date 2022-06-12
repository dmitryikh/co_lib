#pragma once

#include <iostream>
// TODO: switch to c++20's std::source_location when clang is ready.
#include <boost/assert/source_location.hpp>
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>

namespace co::impl {
    inline auto& get_logger() {
        return std::cerr;
    }

    struct FatalStream {
        FatalStream(std::string_view condition, boost::source_location loc)
        {
            get_logger() << "Check failed: \"" << condition << "\""
                    << " funtion " << loc.function_name() << " at " << loc.file_name()
                    << ":" << loc.line() << " ";
        }
        ~FatalStream() {
            get_logger() << "\n";
            get_logger() << boost::stacktrace::stacktrace();
            std::terminate();
        }
    };

    const FatalStream& operator<<(const FatalStream& s, const auto& t) {
        get_logger() << t;
        return s;
    }
}

// Checks an experssion during runtime. Checks are performed in release buils too.
// Example:
//      CO_CHECK(!is_err) << "No error is expected."
#define CO_CHECK(condition) \
    if (!(condition)) \
        ::co::impl::FatalStream(#condition, BOOST_CURRENT_LOCATION)

// Checks an experssion during runtime. Checks are performed only in debug builds.
#ifdef NDEBUG
#define CO_DCHECK(condition) CO_CHECK(true)
#else
#define CO_DCHECK(condition) CO_CHECK(condition)
#endif