#include <co/event.hpp>
#include <co/scheduler.hpp>

namespace co
{

namespace impl
{

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
    return (_event._status == event_status::ok);
}

void event_awaiter::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
{
    _event._waiting_coro = awaiting_coroutine;
    _event._status = event_status::waiting;
    set_this_thread_storage_ptr(nullptr);
}

void event_awaiter::await_resume()
{
    assert(_event._status == event_status::ok);

    set_this_thread_storage_ptr(_thread_storage);

    switch (_event._status)
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

co::stop_callback_func interruptible_event_awaiter::stop_callback_func()
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

void interruptible_event_awaiter::on_timer(void* awaiter_ptr)
{
    assert(awaiter_ptr != nullptr);

    auto& awaiter = *static_cast<interruptible_event_awaiter*>(awaiter_ptr);
    if (awaiter._event._status > event_status::waiting)
    {
        // the waiter of the event is already notified. do nothing
        return;
    }
    awaiter._event._status = event_status::timeout;

    assert(awaiter._event._waiting_coro);
    get_scheduler().ready(awaiter._event._waiting_coro);
}

bool interruptible_event_awaiter::await_ready() const noexcept
{
    assert(_event._status >= event_status::waiting);

    if (_event._status > event_status::waiting)
        return true;

    return false;
}

void interruptible_event_awaiter::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
{
    assert(_event._status == event_status::waiting);
    assert(_event._waiting_coro == nullptr);

    if (_milliseconds_opt.has_value())
        _thread_storage->_timer.set_timer(_milliseconds_opt.value(), on_timer, static_cast<void*>(this));

    _event._waiting_coro = awaiting_coroutine;
    set_this_thread_storage_ptr(nullptr);
}

result<void> interruptible_event_awaiter::await_resume()
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

bool event::notify() noexcept
{
    if (_status >= impl::event_status::ok)
        return false;

    if (_status == impl::event_status::waiting)
        impl::get_scheduler().ready(_waiting_coro);

    _status = impl::event_status::ok;
    return true;
}

impl::event_awaiter event::wait()
{
    if (_status == impl::event_status::waiting)
        throw std::logic_error("event already waiting");

    return impl::event_awaiter(*this);
}

impl::interruptible_event_awaiter event::wait(const until& until)
{
    if (_status == impl::event_status::waiting)
        throw std::logic_error("event already waiting");

    return impl::interruptible_event_awaiter(*this, until);
}

}  // namespace co
