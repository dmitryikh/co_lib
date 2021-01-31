#pragma once

#include <cassert>
#include <co/std.hpp>
#include <co/scheduler.hpp>
#include <co/event.hpp>

namespace co::this_thread
{

template <class Rep, class Period>
task<void> sleep_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token = {})
{
    co_await event{}.wait_for(sleep_duration, token);
}

template <class Clock, class Duration>
task<void> sleep_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = {})
{
    co_await event{}.wait_until(sleep_time, token);
}

} // namespace co::this_thread