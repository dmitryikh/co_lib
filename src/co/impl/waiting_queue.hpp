#pragma once

#include <co/impl/intrusive_list.hpp>
#include <co/event.hpp>

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

    task<void> wait()
    {
        waker w;
        _wakers_list.push_back(w);
        co_await w._event.wait();
        if (w.hook.is_linked())
            w.hook.unlink();
    };

    task<status> wait(const co::stop_token& token)
    {
        return wait_for(std::chrono::milliseconds::max(), token);
    };

    template <class Clock, class Duration>
    task<status> wait_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = {})
    {
        return wait_for(sleep_time - Clock::now(), token);
    }

    template <class Rep, class Period>
    task<status> wait_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token = {})
    {
        waker w;
        _wakers_list.push_back(w);
        const auto status = co_await w._event.wait_for(sleep_duration, token);
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

    bool empty() const
    {
        return _wakers_list.empty();
    }

private:
    wakers_list _wakers_list;
};

}  // co::impl