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
    explicit timed_event_awaiter(timed_event& event)
        : timed_event_awaiter(event, max_milliseconds, std::nullopt)
    {}

    timed_event_awaiter(timed_event& event, const co::until& until)
        : timed_event_awaiter(event, until.milliseconds(), until.token())
    {}

    timed_event_awaiter(timed_event& event,
                        std::optional<int64_t> milliseconds,
                        const std::optional<co::stop_token>& tokenOpt);

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
    timed_event& _event;
    const std::optional<int64_t> _milliseconds_opt;
    std::optional<co::stop_callback> _stop_callback;
};

}  // namespace impl

/// \brief is a interruptable synchronisation primitive for one time notification
///
/// Example:
/// \code
///     co::timed_event e;
///     co::thread([&e]() -> co::func<void>
///     {
///         e.notify();
///     }).detach();
///     auto res = co_await e.wait(co::until(100ms, co::this_thread::stop_token()));
///     if (res == co::cancel)
///         std::cout << "cancelled by stop token\n";
///     else if (res == co::timeout)
///          std::cout << "timeout\n";
///     else
///     {
///         assert(res.is_ok());
///         std::cout << "successfully notified\n";
///     }
/// \endcode
class timed_event
{
    template <typename T>
    friend class impl::event_awaiter;
    friend class impl::timed_event_awaiter;

public:
    /// \brief notify the awaited side that the event is ready.
    ///
    /// Notify can be called many times, but only first time has an effect.
    /// \return true only in case when the notification is successful. That means that awaited side will not block on
    /// wait() or will be resumed without errors (co::result::is_ok() == true);
    bool notify() noexcept
    {
        if (_status >= impl::event_status::ok)
            return false;

        if (_status == impl::event_status::waiting)
            impl::get_scheduler().ready(_waiting_coro);

        _status = impl::event_status::ok;
        return true;
    }

    /// \brief will suspend the co::thread until the notification will be received with notify() method
    ///
    /// wait() won't block if notify() is called in advance. wait() can be called twice, however the second time it will
    /// return immediately. The event can be waited only from one thread
    ///
    /// Usage:
    /// \code
    ///     co_await event.wait();
    /// \endcode
    ///
    /// \return void
    [[nodiscard("co_await me")]] impl::event_awaiter<timed_event> wait()
    {
        if (_status == impl::event_status::waiting)
            throw std::logic_error("event already waiting");

        return impl::event_awaiter<timed_event>(*this);
    };

    /// \brief will suspend the co::thread until the notification will be received with notify() method or interruption
    /// is happened according to the until object
    ///
    /// wait() won't block if notify() is called in advance. wait() can be called twice, however the second time it will
    /// return immediately. The event can be waited only from one thread
    ///
    /// Usage:
    /// \code
    ///     co::result<void> res = co_await event.wait(co::until(100ms, co::this_thread::stop_token()));
    ///     if (res.is_err())
    ///         std::cout << "interrupted\n";
    /// \endcode
    ///
    /// \return void
    [[nodiscard("co_await me")]] impl::timed_event_awaiter wait(const co::until& until)
    {
        if (_status == impl::event_status::waiting)
            throw std::logic_error("event already waiting");

        return impl::timed_event_awaiter(*this, until);
    };

    /// \brief check whether notify() was successfully called
    ///
    /// is_notified() returns false if the event was interrupted or not notified
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

inline timed_event_awaiter::timed_event_awaiter(timed_event& event,
                                                std::optional<int64_t> milliseconds_opt,
                                                const std::optional<co::stop_token>& token_opt)
    : _thread_storage(get_this_thread_storage_ptr())
    , _event(event)
    , _milliseconds_opt(std::move(milliseconds_opt))
{
    // we can't run async code outside of co::thread. Then _thread_storage should be
    // defined in any point of time
    assert(_thread_storage != nullptr);
    assert(_event._waiting_coro == nullptr);
    if (_event._status != event_status::ok)
        _event._status = event_status::waiting;

    if (_milliseconds_opt.has_value() && _milliseconds_opt.value() <= 0)
    {
        _event._status = event_status::timeout;
    }

    if (token_opt.has_value())
        _stop_callback.emplace(*token_opt, stop_callback_func());
}

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
    assert(_event._status >= event_status::waiting);

    if (_event._status > event_status::waiting)
        return true;

    return false;
}

inline void timed_event_awaiter::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
{
    assert(_event._status == event_status::waiting);
    assert(_event._waiting_coro == nullptr);

    if (_milliseconds_opt.has_value())
        _thread_storage->_timer.set_timer(_milliseconds_opt.value(), on_timer, static_cast<void*>(this));

    _event._waiting_coro = awaiting_coroutine;
    set_this_thread_storage_ptr(nullptr);
}

inline result<void> timed_event_awaiter::await_resume()
{
    assert(_event._status > event_status::waiting);

    set_this_thread_storage_ptr(_thread_storage);

    if (_milliseconds_opt.has_value())
        _thread_storage->_timer.stop();  // do not wait the timer anymore

    _event._waiting_coro = nullptr;

    switch (_event._status)
    {
    case event_status::init:
    case event_status::waiting:
        assert(false);
        throw std::logic_error("unexpected status in timed_event_awaiter::await_resume()");
    case event_status::ok:
        return co::ok();
    case event_status::cancel:
        return co::err(co::cancel);
    case event_status::timeout:
        return co::err(co::timeout);
    }
    assert(false);  // unreachable
}

}  // namespace impl

}  // namespace co