#pragma once

#include <co/ts/event.hpp>
#include <co/func.hpp>
#include <co/impl/intrusive_list.hpp>
#include <mutex>

namespace co::ts::impl
{

class waiting_queue;

class waker
{
    friend class waiting_queue;

public:
    waker() = default;

    // Returns true if the event has been notified by this call.
    bool wake() noexcept
    {
        return _event.notify();
    }

private:
    event _event;
    ::co::impl::intrusive_list_hook hook;
};

// It's not thread safe on it's own. The wait methods accept unique_lock for
// syncronisation
class waiting_queue
{
    using wakers_list = co::impl::intrusive_list<waker, &waker::hook>;

public:
    waiting_queue() = default;

    // to be able to move, for example co::condition_variable, we need to define
    // move functions
    waiting_queue(const waiting_queue&) = delete;
    waiting_queue(waiting_queue&&) = default;

    waiting_queue& operator=(const waiting_queue&) = delete;
    waiting_queue& operator=(waiting_queue&&) = default;

    co::func<void> wait(std::unique_lock<std::mutex>& lk);
    void blocking_wait(std::unique_lock<std::mutex>& lk);

    /// \brief notify one of the waiters. Returns true if the waiter was successfully notified
    // Must be called under the mutex.
    bool notify_one();

    /// \brief notify all of the waiters. All waiting co::threads will be resumed
    // Must be called under the mutex.
    void notify_all();

    [[nodiscard]] bool empty() const
    {
        return _wakers_list.empty();
    }

    ~waiting_queue()
    {
        assert(empty());
    }

private:
    wakers_list _wakers_list;
};

}  // namespace co::ts::impl