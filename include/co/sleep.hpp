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
    // NOTE: result will be always co::timeout
    auto _ = co_await timed_event{}.wait({ sleep_duration });
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
    // NOTE: result will be always co::timeout
    auto _ = co_await timed_event{}.wait({ sleep_time });
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