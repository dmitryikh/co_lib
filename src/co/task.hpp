#pragma once

#include <co/std.hpp>
#include <co/impl/shared_state.hpp>
#include <co/impl/awaitable_base.hpp>

namespace co
{

template <typename T, bool>
class task;

class thread;

namespace impl
{

template <typename T, bool self_destroy>
class task_promise
{
public:
    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        return symmetric_transfer_awaitable{ _continuation, self_destroy };
    }

    void set_continuation(std::coroutine_handle<> continuation) noexcept
    {
        _continuation = continuation;
    }

    task<T, self_destroy> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<task_promise<T, self_destroy>>;
        return task<T, self_destroy>{ coroutine_handle::from_promise(*this) };
    }

    void unhandled_exception()
    {
        _state.set_exception(std::current_exception());
    }

    void return_value(T value)
    {
        _state.set_value(std::move(value));
    }

    shared_state<T>& state()
    {
        return _state;
    }

private:
    std::coroutine_handle<> _continuation;
    shared_state<T> _state;
};

} // namespace impl


template<typename T, bool self_destroy = false>
class task
{
    friend class thread;
public:
    using promise_type = impl::task_promise<T, self_destroy>;

private:

    class awaitable : public co::impl::awaitable_base
    {
        using base = co::impl::awaitable_base;
    public:
        awaitable(std::coroutine_handle<promise_type> coroutine) noexcept
            : _coroutine(coroutine)
        {}

        bool await_ready() const noexcept
        {
            return !_coroutine || _coroutine.done();
        }

        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> continuation) noexcept
        {
            _coroutine.promise().set_continuation(continuation);
            // base::await_suspend(continuation);
            return _coroutine;
        }

        T await_resume()
        {
            // base::await_resume();
            return _coroutine.promise().state().value();
        }

    public:
        std::coroutine_handle<promise_type> _coroutine;
    };

public:

    explicit task(std::coroutine_handle<promise_type> coroutine)
        : _coroutine(coroutine)
    {}

    task(task&& t) noexcept
        : task(t._coroutine)
    {
        t._coroutine = nullptr;
    }

    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task& operator=(task&& other) = delete;

    ~task()
    {
        if (_coroutine && !self_destroy)
            _coroutine.destroy();
    }

    auto operator co_await() const
    {
        return awaitable{ _coroutine };
    }

private:
    std::coroutine_handle<promise_type> _coroutine;
};

namespace impl
{

template <bool self_destroy>
class task_promise<void, self_destroy>
{
public:
    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        return symmetric_transfer_awaitable{ _continuation, self_destroy };
    }

    void set_continuation(std::coroutine_handle<> continuation) noexcept
    {
        _continuation = continuation;
    }

    task<void, self_destroy> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<task_promise<void, self_destroy>>;
        return task<void, self_destroy>{ coroutine_handle::from_promise(*this) };
    }

    void unhandled_exception()
    {
        _state.set_exception(std::current_exception());
    }

    void return_void()
    {
        _state.set_value();
    }

    shared_state<void>& state()
    {
        return _state;
    }

private:
    std::coroutine_handle<> _continuation;
    shared_state<void> _state;
};

} // namespace impl

}