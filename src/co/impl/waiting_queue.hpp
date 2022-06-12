#pragma once

#include <chrono>

#include <co/event.hpp>
#include <co/func.hpp>
#include <co/impl/intrusive_list.hpp>

namespace co::impl
{

template <bool ThreadSafe>
class waiting_queue_base;

template <bool ThreadSafe>
class waker
{
    friend class waiting_queue_base<ThreadSafe>;

public:
    waker() = default;

    bool wake() noexcept
    {
        return _event.notify();
    }

private:
    event_base<ThreadSafe> _event;
    intrusive_list_hook hook;
};

// NOTE: The thread-safe versions of waiting_queue is guarded by an external mutex.
// The external mutex is passed to the corresponding class methods.
// The notification methods must be called under the same mutex.
template <bool ThreadSafe>
class waiting_queue_base
{
    using wakers_list = intrusive_list<waker<ThreadSafe>, &waker<ThreadSafe>::hook>;

public:
    waiting_queue_base() = default;

    // to be able to move, for example co::condition_variable, we need to define
    // move functions
    waiting_queue_base(const waiting_queue_base&) = delete;
    waiting_queue_base(waiting_queue_base&&) = default;

    waiting_queue_base& operator=(const waiting_queue_base&) = delete;
    waiting_queue_base& operator=(waiting_queue_base&&) = default;

    /// \brief unconditionally waits until been notified
    co::func<void> wait() requires(!ThreadSafe)
    {
        waker<ThreadSafe> w;
        _wakers_list.push_back(w);
        co_await w._event.wait();
        if (w.hook.is_linked())
            w.hook.unlink();
    }

    template <typename Mutex>
    co::func<void> wait(std::unique_lock<Mutex>& lk)
    {
        CO_DCHECK(lk.owns_lock());
        waker<ThreadSafe> w;
        _wakers_list.push_back(w);
        lk.unlock();
        // TODO: add random sleep here for data race tests
        co_await w._event.wait();
        lk.lock();
        if (w.hook.is_linked())
            w.hook.unlink();
    }

    /// \brief waits until been notified or interruption occurs based on until object
    co::func<co::result<void>> wait(const co::until& until) requires(!ThreadSafe)
    {
        waker<ThreadSafe> w;
        _wakers_list.push_back(w);
        auto status = co_await w._event.wait(until);
        if (w.hook.is_linked())
            w.hook.unlink();
        co_return status;
    }

    template <typename Mutex>
    co::func<co::result<void>> wait(std::unique_lock<Mutex>& lk,
                                    const co::until& until)
    {
        CO_DCHECK(lk.owns_lock());
        waker<ThreadSafe> w;
        _wakers_list.push_back(w);
        lk.unlock();
        auto status = co_await w._event.wait(until);
        lk.lock();
        if (w.hook.is_linked())
            w.hook.unlink();
        co_return status;
    }

    // Blocks the current std::thread until the waker is notified.
    template <typename Mutex>
    void blocking_wait(std::unique_lock<Mutex>& lk)
    {
        CO_DCHECK(lk.owns_lock());
        waker<ThreadSafe> w;
        _wakers_list.push_back(w);
        lk.unlock();
        w._event.blocking_wait();
        lk.lock();
        if (w.hook.is_linked())
            w.hook.unlink();
    }

    template <typename Mutex, class Rep, class Period>
    co::result<void> blocking_wait(std::unique_lock<Mutex>& lk,
                                   std::chrono::duration<Rep, Period> timeout)
    {
        CO_DCHECK(lk.owns_lock());
        waker<ThreadSafe> w;
        _wakers_list.push_back(w);
        lk.unlock();
        co::result<void> status = w._event.blocking_wait(timeout);
        lk.lock();
        if (w.hook.is_linked())
            w.hook.unlink();
        return status;
    }

    /// \brief notify one of the waiters. Returns true if the waiter was successfully notified.
    // Must be called under the mutex.
    bool notify_one()
    {
        while (!_wakers_list.empty())
        {
            waker<ThreadSafe>& w = _wakers_list.front();
            _wakers_list.pop_front();
            if (w.wake())
                return true;
        }
        return false;
    }

    /// \brief notify all of the waiters. All waiting co::threads/std::threads will be resumed.
    // Must be called under the mutex.
    void notify_all()
    {
        while (!_wakers_list.empty())
        {
            waker<ThreadSafe>& w = _wakers_list.front();
            _wakers_list.pop_front();
            w.wake();
        }
    }

    // Must be called under the mutex.
    [[nodiscard]] bool empty() const
    {
        return _wakers_list.empty();
    }

    ~waiting_queue_base()
    {
        CO_DCHECK(empty());
    }

private:
    wakers_list _wakers_list;
};

using waiting_queue = waiting_queue_base<false>;
using ts_waiting_queue = waiting_queue_base<true>;

}  // namespace co::impl