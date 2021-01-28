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

        uv_timer_init(get_scheduler().uv_loop(), &timer_req);
        timer_req.data = awaiting_coroutine.address();
        uv_timer_start(&timer_req, on_timer, milliseconds, 0);
    }

    void await_resume() noexcept {}
private:

    static void on_timer(uv_timer_t* timer_req)
    {
        assert(timer_req != nullptr);

        auto coro = std::experimental::coroutine_handle<>::from_address(timer_req->data);
        get_scheduler().ready(coro);
    }

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