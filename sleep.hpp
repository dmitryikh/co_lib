#pragma once

#include "time.hpp"
#include "scheduler.hpp"

class awaitable_sleep_until
{
public:
    explicit awaitable_sleep_until(time_point until)
        : _until(until)
    {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::experimental::coroutine_handle<> awaiting_coroutine) noexcept
    {
        get_scheduler().schedule_at(_until, awaiting_coroutine);
    }

    void await_resume() noexcept {}
private:
    time_point _until;
};

awaitable_sleep_until sleep_until(time_point until)
{
    return awaitable_sleep_until{ until };
}

awaitable_sleep_until sleep_for(duration dur)
{
    return sleep_until(system_clock::now() + dur);
}