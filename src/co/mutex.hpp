#pragma once

#include <mutex> // for scoped_lock
#include <queue>
#include <co/std.hpp>
#include <co/scheduler.hpp>
#include <co/impl/awaitable_base.hpp>

namespace co
{

class mutex;

namespace impl
{

class mutex_awaitable : public awaitable_base
{
    using base = awaitable_base;
public:
    explicit mutex_awaitable(mutex& mtx)
        : _mutex(mtx)
    {}

    bool await_suspend(std::coroutine_handle<> awaiting_coroutine);

    bool await_ready() const noexcept { return false; }

    std::scoped_lock<mutex> await_resume()
    {
        base::await_resume();
        return std::scoped_lock<mutex>{ std::adopt_lock, _mutex };
    }

private:
    mutex& _mutex;
};

} // namespace impl

class mutex
{
    friend class impl::mutex_awaitable;
public:
    impl::mutex_awaitable lock()
    {
        return impl::mutex_awaitable{ *this };
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


        if (_waiting_queue.empty())
        {
            _is_locked = false;
            return;
        }

        auto coro = _waiting_queue.front();
        _waiting_queue.pop();
        co::impl::get_scheduler().ready(coro);
    }
private:
    bool _is_locked = false;
    std::queue<std::coroutine_handle<>> _waiting_queue;
};

namespace impl
{

bool mutex_awaitable::await_suspend(std::coroutine_handle<> awaiting_coroutine)
{
    // NOTE: base::await_suspend needs to be called uncoditionally, because
    // every time will called await_resume
    base::await_suspend(awaiting_coroutine);

    if (_mutex.try_lock())
        return false;

    _mutex._waiting_queue.push(awaiting_coroutine);
    return true;
}

} // namespace impl

} // namespace co