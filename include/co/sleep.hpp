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
    auto _ = co_await timed_event{}.wait_for(sleep_duration);
}

template <class Rep, class Period>
func<result<void>> sleep_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token)
{
    co_return co_await timed_event{}.wait_for(sleep_duration, token);
}

template <class Clock, class Duration>
func<void> sleep_until(std::chrono::time_point<Clock, Duration> sleep_time)
{
    // NOTE: result will be always co::timeout
    auto _ = co_await timed_event{}.wait_until(sleep_time);
}

template <class Clock, class Duration>
func<result<void>> sleep_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token)
{
    co_return co_await timed_event{}.wait_until(sleep_time, token);
}

}  // namespace co::this_thread