#pragma once

#include <chrono>
#include <co/std.hpp>
#include <co/scheduler.hpp>
#include <co/impl/thread_storage.hpp>
#include <co/stop_token.hpp>
#include <co/result.hpp>
#include <co/error_code.hpp>

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

template <bool Interruptible>
class wait_for_awaiter
{
    constexpr static int64_t max_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds::max()).count();
public:
    wait_for_awaiter(event& event)
        : wait_for_awaiter(event, max_milliseconds, impl::dummy_stop_token)
    {}

    wait_for_awaiter(event& event, int64_t milliseconds)
        : wait_for_awaiter(event, milliseconds, impl::dummy_stop_token)
    {}

    wait_for_awaiter(event& event, int64_t milliseconds, const co::stop_token& token)
        : _thread_storage(get_this_thread_storage_ptr())
        , _milliseconds(milliseconds)
        , _event(event)
        , _token(token)
        , _stop_callback(_token, stop_callback_func())
    {
        // we can't run async code outside of co::thread. Then _thread_storage should be
        // defined in any point of time
        assert(_thread_storage != nullptr);
    }

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept;
    std::conditional_t<Interruptible, result<void>, void> await_resume() noexcept;

private:
    co::stop_callback_func stop_callback_func();
    static void on_timer(uv_timer_t* timer_req);

    thread_storage* _thread_storage = nullptr;  // the thread to which the awaiter belongs
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
    friend class impl::wait_for_awaiter<true>;
    friend class impl::wait_for_awaiter<false>;
public:

    bool notify() noexcept
    {
        if (_status >= impl::event_status::ok)
            return false;

        if (_status == impl::event_status::waiting)
            impl::get_scheduler().ready(_waiting_coro);

        _status = impl::event_status::ok;
        return true;
    }

    [[nodiscard("co_await me")]] impl::wait_for_awaiter<false> wait()
    {
        return impl::wait_for_awaiter<false>(*this);
    };

    [[nodiscard("co_await me")]] impl::wait_for_awaiter<true> wait(const co::stop_token& token)
    {
        return wait_for(std::chrono::milliseconds::max(), token);
    };

    template <class Clock, class Duration>
    [[nodiscard("co_await me")]] impl::wait_for_awaiter<true> wait_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = impl::dummy_stop_token)
    {
        return wait_for(sleep_time - Clock::now(), token);
    }

    template <class Rep, class Period>
    [[nodiscard("co_await me")]] impl::wait_for_awaiter<true> wait_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token = impl::dummy_stop_token)
    {
        if (_status == impl::event_status::waiting)
            throw std::logic_error("event already waiting");


        using std::chrono::duration_cast;
        const int64_t milliseconds = duration_cast<std::chrono::milliseconds>(sleep_duration).count();

        return impl::wait_for_awaiter<true>(*this, milliseconds, token);
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
    template <bool Interruptible>
    co::stop_callback_func wait_for_awaiter<Interruptible>::stop_callback_func()
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

    template <bool Interruptible>
    void wait_for_awaiter<Interruptible>::on_timer(uv_timer_t* timer_req)
    {
        assert(timer_req != nullptr);
        assert(timer_req->data != nullptr);

        auto& awaiter = *static_cast<wait_for_awaiter*>(timer_req->data);
        if (awaiter._event._status > event_status::waiting)
        {
            // the waiter of the event is already notified. do nothing
            return;
        }
        awaiter._event._status = event_status::timeout;
        get_scheduler().ready(awaiter._event._waiting_coro);
    }

    template <bool Interruptible>
    bool wait_for_awaiter<Interruptible>::await_ready() const noexcept
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

    template <bool Interruptible>
    void wait_for_awaiter<Interruptible>::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        uv_timer_init(get_scheduler().uv_loop(), &timer_req);
        _event._waiting_coro = awaiting_coroutine;
        _event._status = event_status::waiting;
        timer_req.data = static_cast<void*>(this);
        uv_timer_start(&timer_req, on_timer, _milliseconds, 0);
        set_this_thread_storage_ptr(nullptr);
    }

    /// @brief await_resume specialization for Interruptible = true. In this case we return error_code
    template<>
    result<void> wait_for_awaiter<true>::await_resume() noexcept
    {
        assert(_event._status > event_status::waiting);

        if (_thread_storage)
            set_this_thread_storage_ptr(_thread_storage);

        // NOTE: may be need to set timer_req.data = nullptr to avoid dangling
        // pointer in case of on_timer() will be called after wait_for_awaiter
        // has been deleted
        if (timer_req.data == static_cast<void*>(this))  // check that we got here after await_suspend()
            uv_timer_stop(&timer_req);

        switch(_event._status)
        {
            case event_status::init:
            case event_status::waiting:
                assert(false);
                // TODO: critical logic error
                std::terminate();
            case event_status::ok:
                return co::ok();
            case event_status::cancel:
                return co::cancel;
            case event_status::timeout:
                return co::timeout;
        }
    }

    template<>
    void wait_for_awaiter<false>::await_resume() noexcept
    {
        assert(_event._status == event_status::ok);

        if (_thread_storage)
            set_this_thread_storage_ptr(_thread_storage);

        if (timer_req.data == static_cast<void*>(this))  // check that we got here after await_suspend()
            uv_timer_stop(&timer_req);

        switch(_event._status)
        {
            case event_status::init:
            case event_status::waiting:
            case event_status::cancel:
            case event_status::timeout:
                assert(false);
                // TODO: critical logic error
                std::terminate();
            case event_status::ok:
                return;
        }
    }

}  // namespace impl

} // namespace co