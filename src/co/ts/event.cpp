#include <co/ts/event.hpp>

#include <atomic>
#include <stdexcept>
#include <co/event.hpp>
#include <co/impl/scheduler.hpp>
#include <co/impl/thread_storage.hpp>

namespace co::ts
{
namespace impl
{

using co::impl::get_scheduler;
using co::impl::event_status;
using co::impl::thread_storage;
using co::impl::co_thread_waker;
using co::impl::get_this_thread_storage_ptr;
using co::impl::current_thread_on_resume;
using co::impl::current_thread_on_suspend;
using co::impl::wake_thread;


void thread_notifier::notify()
{
    std::unique_lock lk(_mutex);
    _notified = true;
    _cv.notify_one();
}

void thread_notifier::wait()
{
    std::unique_lock lk(_mutex);
    _cv.wait(lk, [this] { return _notified; });
}

event_awaiter::event_awaiter(event& _event)
    : _thread_storage(get_this_thread_storage_ptr())
    , _event(_event)
{
    // we can't run async code outside of co::thread. Then _thread_storage should be
    // defined in any point of time
    assert(_thread_storage != nullptr);
}

bool event_awaiter::await_ready() const noexcept
{
    return _event._status.load(std::memory_order_relaxed) == event_status::ok;
}

bool event_awaiter::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
{
    // transition init -> waiting
    current_thread_on_suspend(awaiting_coroutine);
    _event._notifier.emplace<co::impl::co_thread_waker>(_thread_storage);

    event_status expected = event_status::init;
    if (_event._status.compare_exchange_strong(expected, event_status::waiting, std::memory_order_acq_rel))
    {
        // set_this_thread_storage_ptr(nullptr);
        return true;
    }

    // _event._status has been already changed from init to ok, let's resume the current coroutine
    assert(_event._status.load(std::memory_order::acquire) == event_status::ok);
    // _event._notifier.emplace<std::monostate>();
    return false;
}

void event_awaiter::await_resume()
{
    event_status status = _event._status.load(std::memory_order::acquire);
    assert(status == event_status::ok);

    // set_this_thread_storage_ptr(_thread_storage);
    current_thread_on_resume(_thread_storage);

    _event._notifier.emplace<std::monostate>();

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

interruptible_event_awaiter::interruptible_event_awaiter(event& event,
                                                         std::optional<int64_t> milliseconds_opt,
                                                         const std::optional<co::stop_token>& token_opt)
    : _thread_storage(get_this_thread_storage_ptr())
    , _event(event)
    , _milliseconds_opt(std::move(milliseconds_opt))
{
    // we can't run async code outside of co::thread. Then _thread_storage should be
    // defined in any point of time
    assert(_thread_storage != nullptr);
    assert(std::holds_alternative<std::monostate>(_event._notifier));
    assert(_event._status.load(std::memory_order::relaxed) != event_status::waiting);

    // Process timeout earlier
    if (_milliseconds_opt.has_value() && _milliseconds_opt.value() <= 0)
    {
        _event.advance_status_and_assert(event_status::init, event_status::timeout, event_status::ok);
    }
    // NOTE: The stop callback might be immediately called here.
    if (token_opt.has_value())
        _stop_callback.emplace(*token_opt, stop_callback_func());
}

co::stop_callback_func interruptible_event_awaiter::stop_callback_func()
{
    return [this]()
    {
        if (_event.advance_status(event_status::init, event_status::cancel))
        {
            // Called before being waited. Changing the state is enough.
            return;
        }
        if (_event.advance_status_and_assert(event_status::waiting, event_status::cancel, event_status::ok))
        {
            assert(std::holds_alternative<co_thread_waker>(_event._notifier));
            std::get<co_thread_waker>(_event._notifier).wake();
        }
    };
}

void interruptible_event_awaiter::on_timer(void* awaiter_ptr)
{
    assert(awaiter_ptr != nullptr);

    auto& awaiter = *static_cast<interruptible_event_awaiter*>(awaiter_ptr);
    event& event = awaiter._event;
    if (event.advance_status(event_status::init, event_status::timeout))
    {
        // Called before being waited. Changing the state is enough.
        // It's harmless but shoudn't happen.
        assert(false);
        return;
    }
    if (event.advance_status(event_status::waiting, event_status::timeout))
    {
        assert(std::holds_alternative<co_thread_waker>(event._notifier));
        std::get<co_thread_waker>(event._notifier).wake();
    }
}

bool interruptible_event_awaiter::await_ready() const noexcept
{
    return _event._status.load(std::memory_order_relaxed) > event_status::waiting;
}

bool interruptible_event_awaiter::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
{
    assert(_event._status.load(std::memory_order_relaxed) != event_status::waiting);
    // assert(_event._notifier.is_noop());

    // transition init -> waiting
    current_thread_on_suspend(awaiting_coroutine);
    _event._notifier.emplace<co_thread_waker>(_thread_storage);

    if (_event.advance_status(event_status::init, event_status::waiting))
    {
        if (_milliseconds_opt.has_value())
            _thread_storage->_timer.set_timer(_milliseconds_opt.value(), on_timer, static_cast<void*>(this));
        // set_this_thread_storage_ptr(nullptr);
        return true;
    }

    // _event._status has been already changed, let's resume the current coroutine
    return false;
}

result<void> interruptible_event_awaiter::await_resume()
{
    event_status status = _event._status.load(std::memory_order::acquire);
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


}  // namespace impl

// can be called from any thread
bool event::notify()
{
    // possible transitions: init->ok OR waiting->ok
    using co::impl::event_status;

    if (advance_status(event_status::init, event_status::ok))
    {
        // the event has been successfully notified before it had been awaited
        return true;
    }

    if (advance_status(event_status::waiting, event_status::ok))
    {
        if (std::holds_alternative<co::impl::co_thread_waker>(_notifier))
        {
            std::get<co::impl::co_thread_waker>(_notifier).wake();
        }
        else if (std::holds_alternative<impl::thread_notifier>(_notifier))
        {
            std::get<impl::thread_notifier>(_notifier).notify();
        }
        else
        {
            throw std::logic_error("unexpected _notifier variant");
        }
        return true;
    }
    // The event has been already notified.
    assert(_status.load(std::memory_order::acquire) > event_status::waiting);
    return false;
}

impl::event_awaiter event::wait()
{
    if (_status.load(std::memory_order_relaxed) == impl::event_status::waiting)
        throw std::logic_error("event already waiting");

    return impl::event_awaiter(*this);
}

impl::interruptible_event_awaiter event::wait(const co::until& until)
{
    if (_status == impl::event_status::waiting)
        throw std::logic_error("event already waiting");

    return impl::interruptible_event_awaiter(*this, until);
}

void event::blocking_wait()
{
    using co::impl::event_status;

    event_status status = _status.load(std::memory_order_relaxed);
    if (status == event_status::waiting)
        throw std::logic_error("event already waiting");

    if (status == event_status::ok)
        return;

    _notifier.emplace<impl::thread_notifier>();

    if (advance_status(event_status::init, event_status::waiting))
    {
        std::get<impl::thread_notifier>(_notifier).wait();
    }
    assert(_status.load(std::memory_order::acquire) > event_status::waiting);
}

}  // namespace co::ts
