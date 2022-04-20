#pragma once

#include <string>
#include <co/func.hpp>
#include <co/std.hpp>
#include <co/until.hpp>
#include <co/impl/timer.hpp>

namespace co
{

template <bool ThreadSafe>
class event_base;

using event = event_base<false>;

namespace impl
{

struct thread_storage;
class thread_func;

class thread_func_promise : public func_promise_base<void>
{
public:
    thread_func get_return_object() noexcept;

    auto final_suspend() noexcept
    {
        return std::suspend_never{};
    }

    void return_void()
    {
        this->_state.set_value();
    }
};

class [[nodiscard]] thread_func
{
public:
    using promise_type = thread_func_promise;

public:
    auto operator co_await() const
    {
        return other_func_awaiter<promise_type>{ _coroutine };
    }

    std::coroutine_handle<promise_type> _coroutine;
};


/// \brief wrap func into a separate co::thread execution context
/// \param func the work need to be concurrently run
/// \param finish signal that the current thread is finished
/// \param thread_storage current co::thread local storage
inline thread_func create_thread_main_func(func<void> func,
                                           std::shared_ptr<event> finish,
                                           std::shared_ptr<thread_storage> thread_storage);

}  // namespace impl

/// \brief represents a task, running concurrently in the event loop
///
/// can be in two stated: to be joined or detached.
/// Usage:
/// \code
///     auto th = co::thread([]() -> co::func<void>
///     {
///         co_await co::this_thread::sleep_for(100ms);
///     });
///     co_await th.join();
/// \endcode
///
/// \code
///     co::thread([]() -> co::func<void>
///     {
///         co_await co::this_thread::sleep_for(100ms);
///     }).detached();
/// \endcode
class thread
{
public:
    template <FuncLambdaConcept F>
    explicit thread(F&& f, const std::string& thread_name = "")
        : thread(co::invoke(std::forward<F>(f)), thread_name)
    {}

    explicit thread(func<void>&& func, const std::string& thread_name = "");

    ~thread()
    {
        if (!_detached && !is_joined())
        {
            // TODO: terminate is to offensive, diagnostic needed
            // std::terminate();
        }
    }

    /// \brief detach the thread. The thread no more need to be joined before destruction
    void detach()
    {
        _detached = true;
    }

    /// \brief waits until the thread will be finished
    co::func<void> join();

    /// \brief waits until the thread will be finished. Can be interrupted by until conditions
    co::func<co::result<void>> join(co::until until);

    /// \brief checks whether the thread is already joined
    [[nodiscard]] bool is_joined() const;

    /// \brief get a stop source object of the current thread
    [[nodiscard]] co::stop_source get_stop_source() const;

    /// \brief get a stop token object of the current thread
    /// Example:
    /// \code
    ///     auto th = co::thread([]()
    ///     {
    ///         co_await co::this_thread::sleep_for(100ms, co::this_thread::stop_token());
    ///     });
    ///     co_await co::this_thread::sleep_for(50ms);
    ///     th.request_stop();
    ///     co_await th.join();
    /// \endcode
    [[nodiscard]] co::stop_token get_stop_token() const;

    /// \brief set a stop signal for stop source object of the thread
    /// Equivalent:
    /// \code
    ///     thread.get_stop_source().request_stop();
    /// \endcode
    void request_stop() const;

private:
    static inline uint64_t id = 0;

    bool _detached = false;
    std::shared_ptr<impl::thread_storage> _thread_storage_ptr;
    std::shared_ptr<event> _event_ptr;
    impl::thread_func _thread_func;
};


}  // namespace co
