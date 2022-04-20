#pragma once

#include <mutex>
#include <condition_variable>
#include <variant>
#include <chrono>

#include <co/event.hpp>
#include <co/impl/thread_storage.hpp>
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

struct coroutine_notifier
{
    coroutine_notifier(const std::coroutine_handle<>& coro);
    void notify();

    uv_async_ptr _async_ptr;
};

// Notify another thread about an event
struct thread_notifier
{
    void notify();
    void wait();

    template <class Rep, class Period>
    bool wait(std::chrono::duration<Rep, Period> timeout)
    {
        std::unique_lock lk(_mutex);
        _cv.wait_for(lk, timeout, [this] { return _notified; });
        return _notified;
    }

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

// Possible transitions:
// 1. init -> waiting
// 2. init -> canceled (if stop_token was triggered before await)
// 3. init -> timeout (if times up before await)
// 4. init -> ok (if the notify was triggered before await)
// 5. waiting -> canceled
// 6. waiting -> timeout
// 7. waiting -> ok
class interruptible_event_awaiter
{
public:

    interruptible_event_awaiter(event& event,
                                std::optional<int64_t> milliseconds_opt,
                                const std::optional<co::stop_token>& token_opt);

    interruptible_event_awaiter(event& event, const co::until& until)
        : interruptible_event_awaiter(event, until.milliseconds(), until.token())
    {}

    interruptible_event_awaiter& operator=(const interruptible_event_awaiter&) = delete;
    interruptible_event_awaiter& operator=(interruptible_event_awaiter&&) = delete;
    interruptible_event_awaiter(interruptible_event_awaiter&&) = delete;
    interruptible_event_awaiter(const interruptible_event_awaiter&) = delete;

    bool await_ready() const noexcept;
    bool await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept;
    result<void> await_resume();

private:
    co::stop_callback_func stop_callback_func();
    static void on_timer(void* awaiter_ptr);

    co::impl::thread_storage* _thread_storage = nullptr;  // the thread to which the awaiter belongs
    event& _event;
    const std::optional<int64_t> _milliseconds_opt;
    std::optional<co::stop_callback> _stop_callback;
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
    friend class impl::interruptible_event_awaiter;
    using clock_type = std::chrono::steady_clock;
public:
    // TODO: event shouldn't be copyable
    // event& operator=(const event&) = delete;
    // event& perator=(event&&) = default;
    // event(const event&) = delete;
    // event(event&&) = default;

    bool notify();

    [[nodiscard("co_await me")]] impl::event_awaiter wait();

    // NOTE: can be interrupted with co::stop_token only in the same event loop
    [[nodiscard("co_await me")]] impl::interruptible_event_awaiter wait(const co::until& until);

    void blocking_wait();

    template <class Clock, class Duration>
    result<void> blocking_wait(std::chrono::time_point<Clock, Duration> deadline)
    {
        const auto deadline_in_clock_type = ::co::impl::time_point_conv<clock_type>(deadline);
        const auto timeout = deadline_in_clock_type - clock_type::now();
        return blocking_wait(timeout);
    }

    template <class Rep, class Period>
    result<void> blocking_wait(std::chrono::duration<Rep, Period> timeout)
    {
        using co::impl::event_status;

        event_status status = _status.load(std::memory_order_relaxed);
        if (status == event_status::waiting)
            throw std::logic_error("event already waiting");

        if (status < event_status::waiting)
        {
            _notifier.emplace<impl::thread_notifier>();

            if (advance_status(event_status::init, event_status::waiting))
            {
                const auto no_timeout = std::get<impl::thread_notifier>(_notifier).wait(timeout);
                if (!no_timeout)
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

    bool is_notified() const
    {
        return _status.load(std::memory_order_relaxed) == co::impl::event_status::ok;
    }
private:
    // TODO: advance_status might return the current status.
    bool advance_status(co::impl::event_status expected, co::impl::event_status wanted)
    {
        return _status.compare_exchange_strong(expected, wanted, std::memory_order_acq_rel);
    }

private:
    std::atomic<co::impl::event_status> _status = co::impl::event_status::init;
    std::variant<std::monostate, impl::coroutine_notifier, impl::thread_notifier> _notifier;
};

}  // namespace co
