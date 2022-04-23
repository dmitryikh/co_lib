#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>

#include <co/result.hpp>
#include <co/status_code.hpp>
#include <co/std.hpp>
#include <co/stop_token.hpp>
#include <co/until.hpp>
#include <co/impl/thread_storage.hpp>

namespace co
{

template <bool ThreadSafe>
class event_base;

namespace impl
{

struct thread_storage;

enum class event_status
{
    init,
    waiting,
    ok,  // actually means successfully notified
    cancel,
    timeout
};

enum class waker_type
{
    not_set,
    std_thread,
    co_thread
};

struct std_thread_event_data
{
    std::mutex _mutex;
    std::condition_variable _cv;
    bool _notified = false;
};

template <bool ThreadSafe>
class event_awaiter
{
public:
    explicit event_awaiter(event_base<ThreadSafe>& _event)
    : _thread_storage(get_this_thread_storage_ptr())
    , _event(_event)
    {
        // we can't run async code outside of co::thread. Then _thread_storage should be
        // defined in any point of time
        assert(_thread_storage != nullptr);
        assert(_event._waker_type == waker_type::not_set);
    }

    event_awaiter& operator=(const event_awaiter&) = delete;
    event_awaiter& operator=(event_awaiter&&) = delete;
    event_awaiter(event_awaiter&&) = delete;
    event_awaiter(const event_awaiter&) = delete;

    bool await_ready() const noexcept
    {
        return _event._status.load(std::memory_order_relaxed) == event_status::ok;
    }

    bool await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        // transition init -> waiting
        current_thread_on_suspend(awaiting_coroutine);
        _event._waker_data = static_cast<void*>(_thread_storage);
        _event._waker_type = waker_type::co_thread;

        if (_event.advance_status(event_status::init, event_status::waiting))
        {
            return true;
        }

        // _event._status has been already changed from init to ok, let's resume the current coroutine
        assert(_event._status.load(std::memory_order::acquire) == event_status::ok);
        return false;
    }

    void await_resume()
    {
        event_status status = _event._status.load( ThreadSafe ? std::memory_order::acquire : std::memory_order::relaxed);
        assert(status == event_status::ok);

        current_thread_on_resume(_thread_storage);

        switch (status)
        {
        case event_status::init:
        case event_status::waiting:
        case event_status::cancel:
        case event_status::timeout:
            assert(false);
            throw std::logic_error("unexpected status in event_awaiter::await_resume()");
        case event_status::ok:
            return;
        }
        assert(false);  // unreachable
    }

private:
    thread_storage* _thread_storage = nullptr;  // the co::thread to which the awaiter belongs
    event_base<ThreadSafe>& _event;
};

template <bool ThreadSafe>
class interruptible_event_awaiter
{
    interruptible_event_awaiter(event_base<ThreadSafe>& event,
                                std::optional<int64_t> milliseconds_opt,
                                const std::optional<co::stop_token>& token_opt)
    : _thread_storage(get_this_thread_storage_ptr())
    , _event(event)
    , _milliseconds_opt(std::move(milliseconds_opt))
    {
        // we can't run async code outside of co::thread. Then _thread_storage should be
        // defined in any point of time
        assert(_thread_storage != nullptr);
        assert(_event._waker_type == waker_type::not_set);
        assert(_event._status.load(std::memory_order::relaxed) != event_status::waiting);

        // Process timeout earlier
        if (_milliseconds_opt.has_value() && _milliseconds_opt.value() <= 0)
        {
            _event.advance_status(event_status::init, event_status::timeout);
        }
        // NOTE: The stop callback might be immediately called here.
        if (token_opt.has_value())
            _stop_callback.emplace(*token_opt, stop_callback_func());
    }

public:
    interruptible_event_awaiter(event_base<ThreadSafe>& event, const co::until& until)
        : interruptible_event_awaiter(event, until.milliseconds(), until.token())
    {}

    interruptible_event_awaiter& operator=(const interruptible_event_awaiter&) = delete;
    interruptible_event_awaiter& operator=(interruptible_event_awaiter&&) = delete;
    interruptible_event_awaiter(interruptible_event_awaiter&&) = delete;
    interruptible_event_awaiter(const interruptible_event_awaiter&) = delete;

    bool await_ready() const noexcept
    {
        return _event._status.load(std::memory_order_relaxed) > event_status::waiting;
    }

    bool await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
    {
        assert(_event._status.load(std::memory_order_relaxed) != event_status::waiting);

        // transition init -> waiting
        current_thread_on_suspend(awaiting_coroutine);
        _event._waker_data = static_cast<void*>(_thread_storage);
        _event._waker_type = waker_type::co_thread;

        if (_event.advance_status(event_status::init, event_status::waiting))
        {
            if (_milliseconds_opt.has_value())
                _thread_storage->_timer.set_timer(_milliseconds_opt.value(), on_timer, static_cast<void*>(this));
            return true;
        }

        // _event._status has been already changed, let's resume the current coroutine
        return false;
    }
    result<void> await_resume()
    {
        event_status status = _event._status.load( ThreadSafe ? std::memory_order::acquire : std::memory_order::relaxed);
        assert(status > event_status::waiting);

        current_thread_on_resume(_thread_storage);

        if (_milliseconds_opt.has_value())
            _thread_storage->_timer.stop();  // do not wait the timer anymore

        switch (status)
        {
        case event_status::init:
        case event_status::waiting:
            assert(false);
            throw std::logic_error("unexpected status in interruptible_event_awaiter::await_resume()");
        case event_status::ok:
            return co::ok();
        case event_status::cancel:
            return co::err(co::cancel);
        case event_status::timeout:
            return co::err(co::timeout);
        }
        assert(false);  // unreachable
    }

private:
    co::stop_callback_func stop_callback_func()
    {
        return [this]()
        {
            if (_event.advance_status(event_status::init, event_status::cancel))
            {
                // Called before being waited. Changing the state is enough.
                return;
            }
            if (_event.advance_status(event_status::waiting, event_status::cancel))
            {
                assert(_event._waker_type == waker_type::co_thread);
                assert(_event._waker_data != nullptr);
                wake_thread(static_cast<thread_storage*>(_event._waker_data));
            }
        };
    }

    static void on_timer(void* awaiter_ptr)
    {
        assert(awaiter_ptr != nullptr);

        auto& awaiter = *static_cast<interruptible_event_awaiter*>(awaiter_ptr);
        event_base<ThreadSafe>& event = awaiter._event;
        if (event.advance_status(event_status::init, event_status::timeout))
        {
            // Called before being waited. Changing the state is enough.
            // It's harmless but shoudn't happen.
            assert(false);
            return;
        }
        if (event.advance_status(event_status::waiting, event_status::timeout))
        {
            assert(event._waker_type == waker_type::co_thread);
            assert(event._waker_data != nullptr);
            wake_thread(static_cast<thread_storage*>(event._waker_data));
        }
    }

    thread_storage* _thread_storage = nullptr;  // the thread to which the awaiter belongs
    event_base<ThreadSafe>& _event;
    const std::optional<int64_t> _milliseconds_opt;
    std::optional<co::stop_callback> _stop_callback;
};

}  // namespace impl

template <bool ThreadSafe>
class event_base
{
    friend class impl::event_awaiter<ThreadSafe>;
    friend class impl::interruptible_event_awaiter<ThreadSafe>;

    // TODO: Event can't be moved because timer use a pointer to this.

public:
    /// \brief notify the awaited side that the event is ready.
    ///
    /// Notify can be called many times, but only first time has an effect.
    /// \return true only in case when the notification is successful. That means that awaited side will not block on
    /// wait() or will be resumed without errors (co::result::is_ok() == true);
    bool notify() noexcept;

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
    [[nodiscard("co_await me")]] impl::event_awaiter<ThreadSafe> wait()
    {
        if (_status.load(std::memory_order_relaxed) == impl::event_status::waiting)
            throw std::logic_error("event already waiting");

        return impl::event_awaiter<ThreadSafe>(*this);
    }

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
    [[nodiscard("co_await me")]] impl::interruptible_event_awaiter<ThreadSafe> wait(const co::until& until)
    {
        if (_status == impl::event_status::waiting)
            throw std::logic_error("event already waiting");

        return impl::interruptible_event_awaiter<ThreadSafe>(*this, until);
    }

    void blocking_wait() requires (ThreadSafe == true);

    template <class Rep, class Period>
    result<void> blocking_wait(std::chrono::duration<Rep, Period> timeout) requires (ThreadSafe == true);

    /// \brief check whether notify() was successfully called
    ///
    /// is_notified() returns false if the event was interrupted or not notified
    bool is_notified() const
    {
        return _status.load(std::memory_order_relaxed) == co::impl::event_status::ok;
    }
private:
    bool advance_status(co::impl::event_status expected, co::impl::event_status wanted) requires (ThreadSafe == true)
    {
        return _status.compare_exchange_strong(expected, wanted, std::memory_order_acq_rel);
    }

    bool advance_status(co::impl::event_status expected, co::impl::event_status wanted) requires (ThreadSafe == false)
    {
        if (_status.load(std::memory_order::relaxed) == expected)
        {
            _status.store(wanted, std::memory_order::relaxed);
            return true;
        }
        return false;
    }

private:
    std::atomic<co::impl::event_status> _status = impl::event_status::init;
    void* _waker_data = nullptr;
    impl::waker_type _waker_type = impl::waker_type::not_set;
};

template <bool ThreadSafe>
bool event_base<ThreadSafe>::notify() noexcept
{
    // possible transitions: init->ok OR waiting->ok
    using ::co::impl::event_status;

    if (advance_status(event_status::init, event_status::ok))
    {
        // the event has been successfully notified before it had been awaited
        return true;
    }

    if (advance_status(event_status::waiting, event_status::ok))
    {
        assert(_waker_data != nullptr);
        switch(_waker_type)
        {
            case impl::waker_type::co_thread:
            {
                wake_thread(static_cast<impl::thread_storage*>(_waker_data));
                break;
            }
            case impl::waker_type::std_thread:
            {
                auto data = static_cast<impl::std_thread_event_data*>(_waker_data);
                std::unique_lock lk(data->_mutex);
                data->_notified = true;
                data->_cv.notify_one();
                break;
            }
            default:
            {
                assert(false);
            }
        }
        return true;
    }
    // The event has been already notified.
    assert(_status.load(std::memory_order::acquire) > event_status::waiting);
    return false;
}

template <bool ThreadSafe>
void event_base<ThreadSafe>::blocking_wait() requires (ThreadSafe == true)
{
    using ::co::impl::event_status;

    event_status status = _status.load(std::memory_order_relaxed);
    if (status == event_status::waiting)
        throw std::logic_error("event already waiting");

    if (status == event_status::ok)
        return;

    impl::std_thread_event_data data;
    _waker_data = static_cast<void*>(&data);
    _waker_type = impl::waker_type::std_thread;

    if (advance_status(event_status::init, event_status::waiting))
    {
        std::unique_lock lk(data._mutex);
        data._cv.wait(lk, [&data] { return data._notified; });
    }
    assert(_status.load(std::memory_order::acquire) > event_status::waiting);
}


template <bool ThreadSafe>
template <class Rep, class Period>
result<void> event_base<ThreadSafe>::blocking_wait(std::chrono::duration<Rep, Period> timeout) requires (ThreadSafe == true)
{
    using ::co::impl::event_status;

    event_status status = _status.load(std::memory_order_relaxed);
    if (status == event_status::waiting)
        throw std::logic_error("event already waiting");

    if (status < event_status::waiting)
    {
        impl::std_thread_event_data data;
        _waker_data = static_cast<void*>(&data);
        _waker_type = impl::waker_type::std_thread;

        if (advance_status(event_status::init, event_status::waiting))
        {
            std::unique_lock lk(data._mutex);
            data._cv.wait_for(lk, timeout, [&data] { return data._notified; });
            if (!data._notified)
            {
                // Timeout has occured.
                advance_status(event_status::waiting, event_status::timeout);
            }
        }
        status = _status.load(std::memory_order::acquire);
    }
    assert(status > event_status::waiting);

    switch (status)
    {
    case event_status::init:
    case event_status::waiting:
        assert(false);
        throw std::logic_error("unexpected status in interruptible_event_awaiter::await_resume()");
    case event_status::ok:
        return co::ok();
    case event_status::cancel:
        return co::err(co::cancel);
    case event_status::timeout:
        return co::err(co::timeout);
    }
    assert(false);  // unreachable
}

/// \brief is a interruptable synchronisation primitive for one time notification
///
/// Example:
/// \code
///     co::event e;
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
using event = event_base</*ThreadSafe=*/false>;
using ts_event = event_base</*ThreadSafe=*/true>;

}  // namespace co
