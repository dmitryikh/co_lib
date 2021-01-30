#pragma once

#include <cassert>
#include <co/std.hpp>
#include <co/scheduler.hpp>
#include <co/event.hpp>

namespace co::this_thread
{

template <class Rep, class Period>
task<void> sleep_for(std::chrono::duration<Rep, Period> sleep_duration)
{
    co_await event{}.wait_for(sleep_duration);
}

template <class Clock, class Duration>
impl::awaitable_sleep_for sleep_until(std::chrono::time_point<Clock, Duration> sleep_time)
{
    co_await event{}.wait_until(sleep_time);
}

} // namespace co::this_thread