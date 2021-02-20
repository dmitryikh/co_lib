#pragma once

#include <co/func.hpp>
#include <co/result.hpp>
#include <co/stop_token.hpp>
#include <co/timed_event.hpp>

namespace co::this_thread
{

template <class Rep, class Period>
func<void> sleep_for(std::chrono::duration<Rep, Period> sleep_duration)
{
    auto res = co_await timed_event{}.wait({ sleep_duration });
    assert(res == co::timeout);
}

template <class Rep, class Period>
func<result<void>> sleep_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token)
{
    auto res = co_await timed_event{}.wait({ sleep_duration, token });
    if (res == co::timeout)
        co_return co::ok();
    co_return res;
}

template <class Clock, class Duration>
func<void> sleep_until(std::chrono::time_point<Clock, Duration> sleep_time)
{
    auto res = co_await timed_event{}.wait({ sleep_time });
    assert(res == co::timeout);
}

template <class Clock, class Duration>
func<result<void>> sleep_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token)
{
    auto res = co_await timed_event{}.wait({ sleep_time, token });
    if (res == co::timeout)
        co_return co::ok();
    co_return res;
}

}  // namespace co::this_thread