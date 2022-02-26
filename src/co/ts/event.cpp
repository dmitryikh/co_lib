#include <co/ts/event.hpp>

#include <stdexcept>
#include <atomic>
#include <iostream>
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

uv_async_ptr make_and_init_uv_async_handle(const std::coroutine_handle<>& coro)
{
    // The callback will be called in the event loop thread.
    const auto cb = [](uv_async_t *handle)
    {
      auto coro = std::coroutine_handle<>::from_address(handle->data);
      assert(coro != nullptr);
      get_scheduler().ready(coro);
    };
    // TODO: make uv_async_t as part of the event to eliminate the allocation
    auto handle = new uv_async_t;
    uv_async_init(get_scheduler().uv_loop(), handle, cb);
    handle->data = coro.address();
    return uv_async_ptr{ handle };
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
    _event._waiting_coro = awaiting_coroutine;
    _event._async_ptr = make_and_init_uv_async_handle(awaiting_coroutine);

    event_status expected = event_status::init;
    if (_event._status.compare_exchange_strong(expected, event_status::waiting, std::memory_order_acq_rel))
    {
        // the event has been successfully notified before it had been awaited 
        set_this_thread_storage_ptr(nullptr);
        return true;
    }

    // _event._status has been already changed from init to ok, let's resume the current coroutine 
    assert(_event._status.load(std::memory_order_acquire) == event_status::ok);
    _event._waiting_coro = std::coroutine_handle<>{};
    _event._async_ptr.reset();
    return false;
}

void event_awaiter::await_resume()
{
    event_status status = _event._status.load(std::memory_order_acquire);
    assert(status == event_status::ok);

    set_this_thread_storage_ptr(_thread_storage);

    _event._waiting_coro = std::coroutine_handle<>{};
    _event._async_ptr.reset();

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
bool event::notify() noexcept
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
        // the event has been successfully notified after the suspension
        assert(_async_ptr != nullptr);
        // can be called from any thread
        std::cout << "Call the async send" << std::endl;
        uv_async_send(_async_ptr.get());
        return true;
    }

    return false;
}

impl::event_awaiter event::wait()
{
    if (_status.load(std::memory_order_relaxed) == impl::event_status::waiting)
        throw std::logic_error("event already waiting");

    return impl::event_awaiter(*this);
}

}  // namespace co
