#pragma once

#include <string>
#include <co/check.hpp>
#include <co/event.hpp>
#include <co/func.hpp>
#include <co/stop_token.hpp>

namespace co::this_thread
{

/// \brief get the name of the current co::thread. Will terminate if called outside of co::thread
const std::string& name() noexcept;

/// \brief get the id of the current co::thread. Will terminate if called outside of co::thread
uint64_t id() noexcept;

/// \brief get the stop token of the current co::thread. Will terminate if called outside of co::thread
co::stop_token stop_token() noexcept;

/// \brief returns true if current co::thread has been requested to stop. Will terminate if called outside of
/// co::thread
bool stop_requested() noexcept;

/// \brief waits for sleep_duration amount of time
template <class Rep, class Period>
co::func<void> sleep_for(std::chrono::duration<Rep, Period> sleep_duration)
{
    auto res = co_await event{}.wait({ sleep_duration });
    CO_DCHECK(res == co::timeout);
}

/// \brief waits for sleep_duration amount of time. Can be interrupted with stop token
template <class Rep, class Period>
co::func<result<void>> sleep_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token)
{
    auto res = co_await event{}.wait({ sleep_duration, token });
    if (res == co::timeout)
        co_return co::ok();
    co_return res;
}

/// \brief waits until sleep_time
template <class Clock, class Duration>
co::func<void> sleep_until(std::chrono::time_point<Clock, Duration> sleep_time)
{
    auto res = co_await event{}.wait({ sleep_time });
    CO_DCHECK(res == co::timeout);
}

/// \brief waits until sleep_time. Can be interrupted with stop token
template <class Clock, class Duration>
co::func<result<void>> sleep_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token)
{
    auto res = co_await event{}.wait({ sleep_time, token });
    if (res == co::timeout)
        co_return co::ok();
    co_return res;
}

}  // namespace co::this_thread
