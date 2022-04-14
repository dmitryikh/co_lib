#include <co/ts/event.hpp>

#include <stdexcept>
#include <atomic>
#include <co/event.hpp>
#include <co/impl/scheduler.hpp>
#include <co/impl/thread_storage.hpp>

namespace co::ts
{
namespace impl
{

using co::impl::get_scheduler;
using co::impl::event_status;
using co::impl::set_this_thread_storage_ptr;
using co::impl::get_this_thread_storage_ptr;

coroutine_notifier::coroutine_notifier(const std::coroutine_handle<>& coro)
{
    // The callback will be called in the current event loop thread.
    const auto cb = [](uv_async_t *handle)
    {
      auto coro = std::coroutine_handle<>::from_address(handle->data);
      assert(coro != nullptr);
      get_scheduler().ready(coro);
    };

    auto handle = new uv_async_t;
    // TODO: check the return
    uv_async_init(get_scheduler().uv_loop(), handle, cb);
    handle->data = coro.address();
    _async_ptr = uv_async_ptr{ handle };
}

void coroutine_notifier::notify()
{
    assert(_async_ptr != nullptr);
    // can be called from any thread
    uv_async_send(_async_ptr.get());
}

void thread_notifier::notify()
{
    std::unique_lock lk(_mutex);
    _notified = true;
    _cv.notify_one();
}

void thread_notifier::wait()
{
    std::unique_lock lk(_mutex);
    _cv.wait(lk, [this]{ return _notified; });
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
    _event._notifier.emplace<coroutine_notifier>(awaiting_coroutine);

    event_status expected = event_status::init;
    if (_event._status.compare_exchange_strong(expected, event_status::waiting, std::memory_order_acq_rel))
    {
        set_this_thread_storage_ptr(nullptr);
        return true;
    }

    // _event._status has been already changed from init to ok, let's resume the current coroutine 
    assert(_event._status.load(std::memory_order_acquire) == event_status::ok);
    _event._notifier.emplace<std::monostate>();
    return false;
}

void event_awaiter::await_resume()
{
    event_status status = _event._status.load(std::memory_order_acquire);
    assert(status == event_status::ok);

    set_this_thread_storage_ptr(_thread_storage);

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

}  // namespace impl

// can be called from any thread
bool event::notify()
{
    // possible transitions: init->ok OR waiting->ok
    using impl::event_status;

    event_status expected = event_status::init;
    if (_status.compare_exchange_strong(expected, event_status::ok, std::memory_order_acq_rel))
    {
        // the event has been successfully notified before it had been awaited 
        return true;
    }

    expected = event_status::waiting;
    if (_status.compare_exchange_strong(expected, event_status::ok, std::memory_order_acq_rel))
    {
        if (std::holds_alternative<impl::coroutine_notifier>(_notifier))
        {
            std::get<impl::coroutine_notifier>(_notifier).notify();
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
    assert(expected == event_status::ok);
    return false;
}

impl::event_awaiter event::wait()
{
    if (_status.load(std::memory_order_relaxed) == impl::event_status::waiting)
        throw std::logic_error("event already waiting");

    return impl::event_awaiter(*this);
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

    event_status expected = event_status::init;
    if (_status.compare_exchange_strong(expected, event_status::waiting, std::memory_order_acq_rel))
    {
        std::get<impl::thread_notifier>(_notifier).wait();
    }
    else
    {
        assert(expected == event_status::ok);
    }
    assert(_status.load(std::memory_order_acquire) == event_status::ok);
    _notifier.emplace<std::monostate>();
}

}  // namespace co
