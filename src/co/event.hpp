#pragma once

#include <co/result.hpp>
#include <co/status_code.hpp>
#include <co/std.hpp>
#include <co/stop_token.hpp>
#include <co/until.hpp>

namespace co
{

class event;

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

class event_awaiter
{
public:
    explicit event_awaiter(event& _event);

    event_awaiter& operator=(const event_awaiter&) = delete;
    event_awaiter& operator=(event_awaiter&&) = delete;
    event_awaiter(event_awaiter&&) = delete;
    event_awaiter(const event_awaiter&) = delete;

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept;
    void await_resume();

private:
    thread_storage* _thread_storage = nullptr;  // the co::thread to which the awaiter belongs
    event& _event;
};

class interruptible_event_awaiter
{
    interruptible_event_awaiter(event& event,
                                std::optional<int64_t> milliseconds,
                                const std::optional<co::stop_token>& tokenOpt);

public:
    interruptible_event_awaiter(event& event, const co::until& until)
        : interruptible_event_awaiter(event, until.milliseconds(), until.token())
    {}

    interruptible_event_awaiter& operator=(const interruptible_event_awaiter&) = delete;
    interruptible_event_awaiter& operator=(interruptible_event_awaiter&&) = delete;
    interruptible_event_awaiter(interruptible_event_awaiter&&) = delete;
    interruptible_event_awaiter(const interruptible_event_awaiter&) = delete;

    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept;
    result<void> await_resume();

private:
    co::stop_callback_func stop_callback_func();
    static void on_timer(void* awaiter_ptr);

    thread_storage* _thread_storage = nullptr;  // the thread to which the awaiter belongs
    event& _event;
    const std::optional<int64_t> _milliseconds_opt;
    std::optional<co::stop_callback> _stop_callback;
};

}  // namespace impl

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
class event
{
    friend class impl::event_awaiter;
    friend class impl::interruptible_event_awaiter;

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
    [[nodiscard("co_await me")]] impl::event_awaiter wait();
    ;

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
    [[nodiscard("co_await me")]] impl::interruptible_event_awaiter wait(const co::until& until);
    ;

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

}  // namespace co
