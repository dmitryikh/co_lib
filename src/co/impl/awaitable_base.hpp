#pragma once

#include <cassert>
#include <co/std.hpp>
#include <co/impl/thread_storage.hpp>

namespace co::impl
{

class awaitable_base
{
public:
    explicit awaitable_base()
        : _thread_storage(get_this_thread_storage_ptr())
    {
        // we can't run async code outside of co::thread. Then _thread_storage should be
        // defined in any point of time
        assert(_thread_storage != nullptr);
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<>) noexcept
    {
        set_this_thread_storage_ptr(nullptr);
    }

    void await_resume() noexcept
    {
        set_this_thread_storage_ptr(_thread_storage);
    }

private:
    thread_storage* _thread_storage = nullptr;  // the thread to which the awaitable belongs
};

class suspend_always : public awaitable_base
{
    using awaitable_base::awaitable_base;
};

class suspend_never : public awaitable_base
{
public:
    using awaitable_base::awaitable_base;
    bool await_ready() const noexcept { return true; }
};

// NOTE: we don't need awaitable_base here because we don't change the active
// co::thread. We are just returning control to the parent frame
class symmetric_transfer_awaitable
{
public:
    symmetric_transfer_awaitable(std::coroutine_handle<> continuation, bool is_destroy = false)
        : _continuation(continuation)
        , _is_destroy(is_destroy)
    {}

    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> coro) noexcept
    {

        auto continuation = _continuation;
        if (_is_destroy)
            coro.destroy();
        if (continuation)
            return continuation;
        else
        {
            set_this_thread_storage_ptr(nullptr);
            return std::noop_coroutine();
        }
    }

    void await_resume() noexcept
    {
        // NOTE: it's not expected that symmetric_transfer_awaitable will be resumed
        // because symmetric_transfer_awaitable should be used in final_suspend
        assert(true);
    }

private:
    const bool _is_destroy;
    std::coroutine_handle<> _continuation;
};

}