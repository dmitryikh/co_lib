#pragma once

#include <mutex>
#include <condition_variable>
#include <variant>

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

struct coroutine_notifier
{
    coroutine_notifier(const std::coroutine_handle<>& coro);
    void notify();

    uv_async_ptr _async_ptr;
};

struct thread_notifier
{
    void notify();
    void wait();

    std::mutex _mutex;
    std::condition_variable _cv;
    bool _notified = false;
};


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

// std::stop_callback can be invoked in parallel from any thread
// timer callback will be invoked in the loop thread
// notify can be invoked in parallel from any thread
class event
{
    friend class impl::event_awaiter;
public:
    // TODO: event shouldn't be copyable
    // event& operator=(const event&) = delete;
    // event& perator=(event&&) = default;
    // event(const event&) = delete;
    // event(event&&) = default;

    bool notify();

    [[nodiscard("co_await me")]] impl::event_awaiter wait();
    void blocking_wait();

    bool is_notified() const
    {
        return _status.load(std::memory_order_relaxed) == co::impl::event_status::ok;
    }

private:
    std::atomic<co::impl::event_status> _status = co::impl::event_status::init;
    std::variant<std::monostate, impl::coroutine_notifier, impl::thread_notifier> _notifier;
};

}  // namespace co
