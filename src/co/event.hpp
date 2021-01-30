#pragma once

#include <chrono>
#include <co/std.hpp>
#include <co/scheduler.hpp>

namespace co
{
// TODO:
// sleep with cancellation:
// 1. set_on_cancel_callback
// 2. set timer
// 3. on cancel: uv_timer_stop, unset_on_cancel_callback, schedule to ready
// 4. on timer: unset_on_cancel_callback, schedule to ready


class event;

enum class event_status
{
    init,
    waiting,
    ok,  // actualy means successfully notified
    cancel,
    timeout
};

namespace impl
{

class awaitable_wait_for : public awaitable_base
{
    using base = awaitable_base;
public:
    awaitable_wait_for(event& event, int64_t milliseconds)
        : _milliseconds(milliseconds)
        , _event(event)
    {}

    bool await_ready() const noexcept { return _milliseconds <= 0; }

    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept;
    event_status await_resume() noexcept;

private:
    static void on_timer(uv_timer_t* timer_req);

    uv_timer_t timer_req;
    const int64_t _milliseconds;
    event& _event;
};

}  // namespace impl

/// @brief event is simple synchronisation primitive for notification
/// between two co::thread.
/// 
/// @describe event::notify() is used to show that event has
/// happened. event::wait() family of methods are used to wait the event
/// happened. After notification all event::wait() will return immidiately
class event
{
    friend class impl::awaitable_wait_for;
public:

    bool notify()
    {
        if (_status >= event_status::ok)
            return false;

        if (_status == event_status::waiting)
            impl::get_scheduler().ready(_waiting_coro);

        _status = event_status::ok;
        return true;
    }

    impl::awaitable_wait_for wait()
    {
        return wait_for(std::chrono::milliseconds::max());
    };

    template <class Clock, class Duration>
    impl::awaitable_wait_for wait_until(std::chrono::time_point<Clock, Duration> sleep_time)
    {
        return wait_for(sleep_time - Clock::now());
    }

    template <class Rep, class Period>
    impl::awaitable_wait_for wait_for(std::chrono::duration<Rep, Period> sleep_duration)
    {
        if (_status == event_status::ok)
            throw std::logic_error("event already done");

        if (_status == event_status::waiting)
            throw std::logic_error("event already waiting");

        _status = event_status::waiting;

        using std::chrono::duration_cast;
        const int64_t milliseconds = duration_cast<std::chrono::milliseconds>(sleep_duration).count();

        return impl::awaitable_wait_for(*this, milliseconds);
    };

private:
    event_status _status = event_status::init;
    bool _notified = false;
    std::coroutine_handle<> _waiting_coro;
};

namespace impl
{
    void awaitable_wait_for::on_timer(uv_timer_t* timer_req)
    {
        assert(timer_req != nullptr);
        assert(timer_req->data != nullptr);

        auto& awaitable = *static_cast<awaitable_wait_for*>(timer_req->data);
        if (awaitable._event._status > event_status::waiting)
        {
            // the waiter of the event is already notified. do nothing
            return;
        }
        awaitable._event._status = event_status::timeout;
        get_scheduler().ready(awaitable._event._waiting_coro);
    }

    void awaitable_wait_for::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        uv_timer_init(get_scheduler().uv_loop(), &timer_req);
        _event._waiting_coro = awaiting_coroutine;
        timer_req.data = static_cast<void*>(this);
        uv_timer_start(&timer_req, on_timer, _milliseconds, 0);
        base::await_suspend(awaiting_coroutine);
    }

    event_status awaitable_wait_for::await_resume() noexcept
    {
        base::await_resume();
        assert(_event._status > event_status::waiting);
        // just want to be sure that await_resume() is not called when await_ready() return true; 
        assert(!await_ready());

        // NOTE: may be need to set timer_req.data = nullptr to avoid dangling
        // pointer in case of on_timer() will be called after awaitable_wait_for
        // has been deleted
        uv_timer_stop(&timer_req);
        return _event._status;
    }

}  // namespace impl

} // namespace co