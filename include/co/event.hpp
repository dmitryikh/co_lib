#pragma once

#include <co/impl/thread_storage.hpp>
#include <co/scheduler.hpp>
#include <co/std.hpp>

namespace co
{

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

template <typename event>
class event_awaiter
{
public:
    event_awaiter(event& _event);

    event_awaiter& operator=(const event_awaiter&) = delete;
    event_awaiter& operator=(event_awaiter&&) = delete;
    event_awaiter(event_awaiter&&) = delete;
    event_awaiter(const event_awaiter&) = delete;

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept;
    void await_resume();

private:
    thread_storage* _thread_storage = nullptr;  // the thread to which the awaiter belongs
    event& _event;
};

}  // namespace impl

/// @brief simplest not interruptable event between 2 objects
class event
{
    template <typename T>
    friend class impl::event_awaiter;

public:
    bool notify() noexcept
    {
        if (_status == impl::event_status::ok)
            return false;

        if (_status == impl::event_status::waiting)
            impl::get_scheduler().ready(_waiting_coro);

        _status = impl::event_status::ok;
        return true;
    }

    [[nodiscard("co_await me")]] impl::event_awaiter<event> wait()
    {
        return impl::event_awaiter<event>(*this);
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

template <typename event>
event_awaiter<event>::event_awaiter(event& _event)
    : _thread_storage(get_this_thread_storage_ptr())
    , _event(_event)
{
    // we can't run async code outside of co::thread. Then _thread_storage should be
    // defined in any point of time
    assert(_thread_storage != nullptr);
}

template <typename event>
bool event_awaiter<event>::await_ready() const noexcept
{
    return (_event._status == event_status::ok);
}

template <typename event>
void event_awaiter<event>::await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
{
    _event._waiting_coro = awaiting_coroutine;
    _event._status = event_status::waiting;
    set_this_thread_storage_ptr(nullptr);
}

template <typename event>
void event_awaiter<event>::await_resume()
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
}

}  // namespace impl

}  // namespace co