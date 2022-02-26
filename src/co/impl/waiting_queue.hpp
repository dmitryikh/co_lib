#pragma once

#include <co/event.hpp>
#include <co/func.hpp>
#include <co/impl/intrusive_list.hpp>

namespace co::impl
{

class waiting_queue;

class waker
{
    friend class waiting_queue;

public:
    waker() = default;

    bool wake() noexcept
    {
        return _event.notify();
    }

private:
    event _event;
    intrusive_list_hook hook;
};

class waiting_queue
{
    using wakers_list = intrusive_list<waker, &waker::hook>;

public:
    waiting_queue() = default;

    // to be able to move, for example co::condition_variable, we need to define
    // move functions
    waiting_queue(const waiting_queue&) = delete;
    waiting_queue(waiting_queue&&) = default;

    waiting_queue& operator=(const waiting_queue&) = delete;
    waiting_queue& operator=(waiting_queue&&) = default;

    /// \brief unconditionally waits until been notified
    co::func<void> wait();
    ;

    /// \brief waits until been notified or interruption occurs based on until object
    co::func<co::result<void>> wait(const co::until& until);
    ;

    /// \brief notify one of the waiters. Returns true if the waiter was successfully notified
    bool notify_one();

    /// \brief notify all of the waiters. All waiting co::threads will be resumed
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

}  // namespace co::impl