#pragma once

#include <cassert>
#include <co/std.hpp>
#include <co/scheduler.hpp>
#include <co/impl/awaitable_base.hpp>

namespace co
{

namespace impl
{

class awaitable_sleep_for : public awaitable_base
{
    using base = awaitable_base;
public:
    explicit awaitable_sleep_for(int64_t milliseconds)
        : _milliseconds(milliseconds)
    {}

    bool await_ready() const noexcept { return _milliseconds <= 0; }

    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        uv_timer_init(get_scheduler().uv_loop(), &timer_req);
        timer_req.data = awaiting_coroutine.address();
        uv_timer_start(&timer_req, on_timer, _milliseconds, 0);
        base::await_suspend(awaiting_coroutine);
    }

    void await_resume() noexcept
    {
        base::await_resume();
    }
private:

    static void on_timer(uv_timer_t* timer_req)
    {
        assert(timer_req != nullptr);

        auto coro = std::coroutine_handle<>::from_address(timer_req->data);
        get_scheduler().ready(coro);
    }

    uv_timer_t timer_req;
    int64_t _milliseconds;
};

} // namespace impl

namespace this_thread
{

template <class Rep, class Period>
impl::awaitable_sleep_for sleep_for(std::chrono::duration<Rep, Period> sleep_duration)
{
    using std::chrono::duration_cast;
    const int64_t milliseconds = duration_cast<std::chrono::milliseconds>(sleep_duration).count();
    return impl::awaitable_sleep_for{ milliseconds };
}

template <class Clock, class Duration>
impl::awaitable_sleep_for sleep_until(std::chrono::time_point<Clock, Duration> sleep_time)
{
    return sleep_for(sleep_time - Clock::now());
}

} // namespace this_thread

}