#pragma once

#include <co/impl/intrusive_list.hpp>
#include <co/timed_event.hpp>

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
    timed_event _event;
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

    func<void> wait()
    {
        waker w;
        _wakers_list.push_back(w);
        co_await w._event.wait();
        if (w.hook.is_linked())
            w.hook.unlink();
    };

    func<result<void>> wait(const co::until& until)
    {
        waker w;
        _wakers_list.push_back(w);
        auto status = co_await w._event.wait(until);
        if (w.hook.is_linked())
            w.hook.unlink();
        co_return status;
    };

    bool notify_one()
    {
        while (!_wakers_list.empty())
        {
            waker& w = _wakers_list.front();
            _wakers_list.pop_front();
            if (w.wake())
                return true;
        }
        return false;
    }

    void notify_all()
    {
        while (!_wakers_list.empty())
        {
            waker& w = _wakers_list.front();
            _wakers_list.pop_front();
            w.wake();
        }
    }

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