#pragma once

#include <string>
#include <co/event.hpp>
#include <co/stop_token.hpp>

namespace co::this_thread
{

/// \brief get the name of the current co::thread. Will throws an exception if called outside of co::thread
const std::string& name();

/// \brief get the id of the current co::thread. Will throws an exception if called outside of co::thread
uint64_t id();

/// \brief get the stop token of the current co::thread. Will throws an exception if called outside of co::thread
co::stop_token stop_token();

/// \brief returns true if current co::thread has been requested to stop. Will throws an exception if called outside of
/// co::thread
bool stop_requested();

/// \brief waits for sleep_duration amount of time
template <class Rep, class Period>
func<void> sleep_for(std::chrono::duration<Rep, Period> sleep_duration)
{
    auto res = co_await event{}.wait({ sleep_duration });
    assert(res == co::timeout);
}

/// \brief waits for sleep_duration amount of time. Can be interrupted with stop token
template <class Rep, class Period>
func<result<void>> sleep_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token)
{
    auto res = co_await event{}.wait({ sleep_duration, token });
    if (res == co::timeout)
        co_return co::ok();
    co_return res;
}

/// \brief waits until sleep_time
template <class Clock, class Duration>
func<void> sleep_until(std::chrono::time_point<Clock, Duration> sleep_time)
{
    auto res = co_await event{}.wait({ sleep_time });
    assert(res == co::timeout);
}

/// \brief waits until sleep_time. Can be interrupted with stop token
template <class Clock, class Duration>
func<result<void>> sleep_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token)
{
    auto res = co_await event{}.wait({ sleep_time, token });
    if (res == co::timeout)
        co_return co::ok();
    co_return res;
}

}  // namespace co::this_thread
