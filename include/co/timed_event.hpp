#pragma once

#include <chrono>
#include <co/error_code.hpp>
#include <co/event.hpp>
#include <co/impl/thread_storage.hpp>
#include <co/result.hpp>
#include <co/scheduler.hpp>
#include <co/std.hpp>
#include <co/stop_token.hpp>
#include <co/until.hpp>
#include <co/impl/timer-impl.hpp>

namespace co
{

class timed_event;

namespace impl
{

class timed_event_awaiter
{
    constexpr static int64_t max_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds::max()).count();

public:
    timed_event_awaiter(timed_event& event)
        : timed_event_awaiter(event, max_milliseconds, impl::dummy_stop_token)
    {}

    timed_event_awaiter(timed_event& event, const co::until& until)
        : timed_event_awaiter(event, until.milliseconds(), until.token())
    {}

    timed_event_awaiter(timed_event& event, int64_t milliseconds, const co::stop_token& token)
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

    timed_event_awaiter& operator=(const timed_event_awaiter&) = delete;
    timed_event_awaiter& operator=(timed_event_awaiter&&) = delete;
    timed_event_awaiter(timed_event_awaiter&&) = delete;
    timed_event_awaiter(const timed_event_awaiter&) = delete;

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept;
    result<void> await_resume();

private:
    co::stop_callback_func stop_callback_func();
    static void on_timer(void* awaiter_ptr);

    thread_storage* _thread_storage = nullptr;  // the thread to which the awaiter belongs
    const int64_t _milliseconds;
    timed_event& _event;
    const co::stop_token& _token;
    co::stop_callback<> _stop_callback;
};

}  // namespace impl

/// @brief timed_event is simple synchronisation primitive for notification
/// between two co::thread.
///
/// @describe event::notify() is used to show that event has
/// happened. event::wait() family of methods are used to wait the event
/// happened. After notification all event::wait() will return immidiately
class timed_event
{
    template <typename T>
    friend class impl::event_awaiter;
    friend class impl::timed_event_awaiter;

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

    [[nodiscard("co_await me")]] impl::event_awaiter<timed_event> wait()
    {
        return impl::event_awaiter<timed_event>(*this);
    };

    [[nodiscard("co_await me")]] impl::timed_event_awaiter wait(const co::until& until)
    {
        if (_status == impl::event_status::waiting)
            throw std::logic_error("event already waiting");

        return impl::timed_event_awaiter(*this, until);
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

inline co::stop_callback_func timed_event_awaiter::stop_callback_func()
{
    return [this]()
    {
        if (_event._status > event_status::waiting)
        {
            // the waiter of the event is already notified. do nothing
            return;
        }
        _event._status = event_status::cancel;
        if (_event._waiting_coro)
        {
            get_scheduler().ready(_event._waiting_coro);
        }
    };
}

inline void timed_event_awaiter::on_timer(void* awaiter_ptr)
{
    assert(awaiter_ptr != nullptr);

    auto& awaiter = *static_cast<timed_event_awaiter*>(awaiter_ptr);
    if (awaiter._event._status > event_status::waiting)
    {
        // the waiter of the event is already notified. do nothing
        return;
    }
    awaiter._event._status = event_status::timeout;

    assert(awaiter._event._waiting_coro);
    get_scheduler().ready(awaiter._event._waiting_coro);
}

inline bool timed_event_awaiter::await_ready() const noexcept
{
    if (_event._status == event_status::ok)
        return true;

    if (_token.stop_requested())
    {
        _event._status = event_status::cancel;
        return true;
    }

    if (_milliseconds <= 0)
    {
        _event._status = event_status::timeout;
        return true;
    }

    return false;
}

inline void timed_event_awaiter::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
{
    _thread_storage->_timer.set_timer(_milliseconds, on_timer, static_cast<void*>(this));
    _event._waiting_coro = awaiting_coroutine;
    _event._status = event_status::waiting;
    set_this_thread_storage_ptr(nullptr);
}

inline result<void> timed_event_awaiter::await_resume()
{
    assert(_event._status > event_status::waiting);

    set_this_thread_storage_ptr(_thread_storage);
    _thread_storage->_timer.stop();  // do not wait the timer anymore

    switch (_event._status)
    {
    case event_status::init:
    case event_status::waiting:
        assert(false);
        throw std::logic_error("unexpected status in event_awaiter::await_resume()");
    case event_status::ok:
        return co::ok();
    case event_status::cancel:
        return co::err(co::cancel);
    case event_status::timeout:
        return co::err(co::timeout);
    }
}

}  // namespace impl

}  // namespace co