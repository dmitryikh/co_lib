#pragma once

#include <chrono>
#include <co/std.hpp>
#include <co/scheduler.hpp>
#include <co/stop_token.hpp>
#include <co/status.hpp>

namespace co
{

class event;

namespace impl
{

enum class event_status
{
    init,
    waiting,
    ok,  // actualy means successfully notified
    cancel,
    timeout
};

class awaitable_wait_for : public awaitable_base
{
    using base = awaitable_base;
public:
    awaitable_wait_for(event& event, int64_t milliseconds, const co::stop_token& token)
        : _milliseconds(milliseconds)
        , _event(event)
        , _token(token)
        , _stop_callback(_token, stop_callback_func())
    {}

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept;
    status await_resume() noexcept;

private:
    co::stop_callback_func stop_callback_func();
    static void on_timer(uv_timer_t* timer_req);

    uv_timer_t timer_req;
    const int64_t _milliseconds;
    event& _event;
    const co::stop_token& _token;
    co::stop_callback<> _stop_callback;
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
        if (_status >= impl::event_status::ok)
            return false;

        if (_status == impl::event_status::waiting)
            impl::get_scheduler().ready(_waiting_coro);

        _status = impl::event_status::ok;
        return true;
    }

    impl::awaitable_wait_for wait(const co::stop_token& token = {})
    {
        return wait_for(std::chrono::milliseconds::max(), token);
    };

    template <class Clock, class Duration>
    impl::awaitable_wait_for wait_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = {})
    {
        return wait_for(sleep_time - Clock::now(), token);
    }

    template <class Rep, class Period>
    impl::awaitable_wait_for wait_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token = {})
    {
        if (_status == impl::event_status::waiting)
            throw std::logic_error("event already waiting");


        using std::chrono::duration_cast;
        const int64_t milliseconds = duration_cast<std::chrono::milliseconds>(sleep_duration).count();

        return impl::awaitable_wait_for(*this, milliseconds, token);
    };

    bool is_notified() const
    {
        return _status == impl::event_status::ok;
    }

private:
    impl::event_status _status = impl::event_status::init;
    std::coroutine_handle<> _waiting_coro;
};

namespace impl
{
    co::stop_callback_func awaitable_wait_for::stop_callback_func()
    {
        return [this] ()
        {
            if (_event._status > event_status::waiting)
            {
                // the waiter of the event is already notified. do nothing
                return;
            }
            _event._status = event_status::cancel;
            if (_event._waiting_coro)
                get_scheduler().ready(_event._waiting_coro);
        };
    }

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

    bool awaitable_wait_for::await_ready() const noexcept
    {
        if (_event._status == event_status::ok)
            return true;

        if (_milliseconds <= 0)
        {
            _event._status = event_status::timeout;
            return true;
        }

        if (_token.stop_requested())
        {
            _event._status = event_status::cancel;
            return true;
        }

        return false;
    }

    void awaitable_wait_for::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        uv_timer_init(get_scheduler().uv_loop(), &timer_req);
        _event._waiting_coro = awaiting_coroutine;
        _event._status = event_status::waiting;
        timer_req.data = static_cast<void*>(this);
        uv_timer_start(&timer_req, on_timer, _milliseconds, 0);
        base::await_suspend(awaiting_coroutine);
    }

    status awaitable_wait_for::await_resume() noexcept
    {
        base::await_resume();
        assert(_event._status > event_status::waiting);

        // NOTE: may be need to set timer_req.data = nullptr to avoid dangling
        // pointer in case of on_timer() will be called after awaitable_wait_for
        // has been deleted
        if (timer_req.data == static_cast<void*>(this))  // check that we got here after await_suspend()
            uv_timer_stop(&timer_req);

        switch(_event._status)
        {
            case event_status::init:
            case event_status::waiting:
                assert(false);
                return co::undefined;
            case event_status::ok:
                return co::ok;
            case event_status::cancel:
                return co::cancel;
            case event_status::timeout:
                return co::timeout;
        }
    }

}  // namespace impl

} // namespace co