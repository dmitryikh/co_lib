#pragma once

#include <chrono>
#include <co/stop_token.hpp>

namespace co
{

class until
{
public:
    using clock = std::chrono::steady_clock;

    until() = default;

    until(co::stop_token token)
        : _token(std::move(token))
    {}

    template <class Rep, class Period>
    until(std::chrono::duration<Rep, Period> sleep_duration)
        : _until(clock::now() + sleep_duration)
    {}

    template <class Rep, class Period>
    until(std::chrono::duration<Rep, Period> timeout, co::stop_token token)
        : _until(clock::now() + timeout)
        , _token(std::move(token))
    {}

    template <class Rep, class Period>
    until(clock::time_point deadline)
        : _until(deadline)
    {}

    template <class Rep, class Period>
    until(clock::time_point deadline, co::stop_token token)
        : _until(deadline)
        , _token(std::move(token))
    {}

    const co::stop_token& token() const
    {
        return _token;
    }

    const int64_t milliseconds() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(_until - clock::now()).count();
    }

private:
    clock::time_point _until = clock::time_point::max();
    co::stop_token _token = impl::dummy_stop_token;
};

}  // namespace co