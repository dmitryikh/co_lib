#pragma once

#include <co/std.hpp>
#include <co/impl/shared_state.hpp>

namespace co
{

template <typename T>
class task;

namespace impl
{

struct final_awaitable
{
    bool await_ready() const noexcept
    {
        return false;
    }

    template<typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> coro) noexcept
    {
        return coro.promise()._continuation;
    }

    void await_resume() noexcept {}
};

template <typename T>
class task_promise
{
    friend struct final_awaitable;

public:
    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        return final_awaitable{};
    }

    void set_continuation(std::coroutine_handle<> continuation) noexcept
    {
        _continuation = continuation;
    }

    task<T> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<task_promise<T>>;
        return task<T>{ coroutine_handle::from_promise(*this), _state };
    }

    void unhandled_exception()
    {
        _state.set_exception(std::current_exception());
    }

    void return_value(T value)
    {
        _state.set_value(std::move(value));
    }

    shared_state<void>& state()
    {
        return _state;
    }

private:

    std::coroutine_handle<> _continuation;
    shared_state<T> _state;
};

} // namespace impl


template<typename T>
class task
{
public:

    using promise_type = impl::task_promise<T>;

private:

    class awaitable
    {
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
            return _coroutine;
        }

        T await_resume()
        {
            return _coroutine.promise().state().value();
        }

    public:
        std::coroutine_handle<promise_type> _coroutine;
    };

public:

    explicit task(std::coroutine_handle<promise_type> coroutine, impl::shared_state<T>& state)
        : _coroutine(coroutine)
        , _state(state)
    {}

    task(task&& t) noexcept
        : task(t._coroutine, t._state)
    {
        t._coroutine = nullptr;
    }

    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task& operator=(task&& other) = delete;

    ~task()
    {
        if (_coroutine)
            _coroutine.destroy();
    }

    bool is_done() const
    {
        return _state.is_done();
    }

    auto operator co_await() const
    {
        return awaitable{ _coroutine };
    }

private:

    std::coroutine_handle<promise_type> _coroutine;
    impl::shared_state<T>& _state;
};

namespace impl
{

template <>
class task_promise<void>
{
    friend struct final_awaitable;

public:
    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        return final_awaitable{};
    }

    void set_continuation(std::coroutine_handle<> continuation) noexcept
    {
        _continuation = continuation;
    }

    task<void> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<task_promise<void>>;
        return task<void>{ coroutine_handle::from_promise(*this), _state };
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