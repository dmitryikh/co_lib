#pragma once

#include <list>
#include <co/std.hpp>
#include <co/scheduler.hpp>
#include <co/stop_token.hpp>
#include <co/impl/awaitable_base.hpp>

namespace co
{

class mutex;

namespace impl
{
    // TODO: replace with intrusive list
    struct event_node
    {
        event ev;
        std::optional<std::list<event_node*>::iterator> it;
    };

}

class mutex
{
public:
    task<void> lock()
    {
        if (try_lock())
            co_return;

        impl::event_node event;
        event.it = _waiting_queue.insert(_waiting_queue.end(), &event);

        co_await event.ev.wait();
    }

    task<status> lock(const stop_token& token)
    {
        if (try_lock())
            co_return ok;

        impl::event_node event;
        event.it = _waiting_queue.insert(_waiting_queue.end(), &event);

        co_return co_await event.ev.wait(token);
    }

    template <class Rep, class Period>
    task<status> lock_for(std::chrono::duration<Rep, Period> sleep_duration, const stop_token& token = {})
    {
        if (try_lock())
            co_return ok;

        impl::event_node event;
        event.it = _waiting_queue.insert(_waiting_queue.end(), &event);

        co_return co_await event.ev.wait_for(sleep_duration, token);
    }

    ~mutex()
    {
        assert(!_is_locked);
    }

    bool try_lock()
    {
        if (_is_locked)
            return false;

        _is_locked = true;
        return true;
    }

    bool is_locked() const
    {
        return _is_locked;
    }

    void unlock()
    {
        assert(_is_locked);
        // TODO: check that unlock called from the proper coroutine
        if (!_is_locked)
            return;

        while (!_waiting_queue.empty())
        {
            auto ptr = *_waiting_queue.begin();
            assert(ptr != nullptr);
            ptr->it = std::nullopt;
            _waiting_queue.pop_front();
            if (ptr->ev.notify())
                return;
        }
        _is_locked = false;
    }
private:
    bool _is_locked = false;
    std::list<impl::event_node*> _waiting_queue;
};

} // namespace co