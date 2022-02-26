#pragma once

#include <co/event.hpp>
#include <co/impl/uv_handler.hpp>
#include <co/result.hpp>
#include <co/status_code.hpp>
#include <co/std.hpp>

namespace co::ts
{

class event;

namespace impl
{

using uv_async_ptr = co::impl::uv_handle_ptr<uv_async_t>;
uv_async_ptr make_and_init_uv_async_handle(const std::coroutine_handle<>& coro);

class event_awaiter
{
public:
    explicit event_awaiter(event& _event);

    event_awaiter& operator=(const event_awaiter&) = delete;
    event_awaiter& operator=(event_awaiter&&) = delete;
    event_awaiter(event_awaiter&&) = delete;
    event_awaiter(const event_awaiter&) = delete;

    bool await_ready() const noexcept;
    bool await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept;
    void await_resume();

private:
    co::impl::thread_storage* _thread_storage = nullptr;  // the co::thread to which the awaiter belongs
    event& _event;
};

}  // namespace impl

/// \brief is a interruptable synchronisation primitive for one time notification.
/// The notification can be called from different system threads
/// See co::event for usage details.
class event
{
    friend class impl::event_awaiter;

public:
    bool notify() noexcept;

    [[nodiscard("co_await me")]] impl::event_awaiter wait();

    bool is_notified() const
    {
        return _status.load(std::memory_order_relaxed) == co::impl::event_status::ok;
    }

private:
    std::atomic<co::impl::event_status> _status = co::impl::event_status::init;
    std::coroutine_handle<> _waiting_coro;
    impl::uv_async_ptr _async_ptr = nullptr;
};

}  // namespace co
