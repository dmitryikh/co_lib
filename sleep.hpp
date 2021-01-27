#pragma once

#include "time.hpp"
#include "scheduler.hpp"

class awaitable_sleep_for
{
public:
    explicit awaitable_sleep_for(duration dur)
        : _duration(dur)
    {}

    bool await_ready() const noexcept { return _duration <= duration{}; }

    void await_suspend(std::experimental::coroutine_handle<> awaiting_coroutine) noexcept
    {
        int64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(_duration).count();
        get_scheduler().schedule_at(timer_req, milliseconds, awaiting_coroutine);
    }

    void await_resume() noexcept {}
private:
    uv_timer_t timer_req;
    duration _duration;
};

awaitable_sleep_for sleep_for(duration dur)
{
    return awaitable_sleep_for{ dur };
}

awaitable_sleep_for sleep_until(time_point until)
{
    return sleep_for(until - system_clock::now());
}