#pragma once

#include <list>
#include <co/impl/waiting_queue.hpp>
#include <co/scheduler.hpp>
#include <co/std.hpp>
#include <co/stop_token.hpp>
#include <co/until.hpp>

namespace co
{

class mutex
{
public:
    func<void> lock()
    {
        if (try_lock())
            co_return;

        co_await _waiting_queue.wait();
    }

    func<result<void>> lock(co::until until)
    {
        if (try_lock())
            co_return co::ok();

        co_return co_await _waiting_queue.wait(until);
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

        if (!_waiting_queue.notify_one())
            _is_locked = false;
    }

private:
    bool _is_locked = false;
    impl::waiting_queue _waiting_queue;
};

}  // namespace co