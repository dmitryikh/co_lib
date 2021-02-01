#pragma once

#include <cassert>
#include <co/std.hpp>
#include <co/impl/shared_state.hpp>

namespace co
{

class thread;

namespace impl
{

template <typename T, typename FinalAwaiter>
class func_template;

// NOTE: we don't need to change thread_storage_ptr here because we don't change
// the active co::thread. We are just returning control to the parent frame
// NOTE: the frame will be destroyed in func's destructor
class symmetric_transfer_awaiter
{
public:
    symmetric_transfer_awaiter(std::coroutine_handle<> continuation)
        : _continuation(continuation)
    {}

    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept
    {

        return _continuation;
    }

    void await_resume() noexcept
    {
        // NOTE: it's not expected to be resumed because
        // symmetric_transfer_awaiter should be used in final_suspend
        assert(false);
    }

private:
    std::coroutine_handle<> _continuation;
};

class never_awaiter : public std::suspend_never
{
public:
    never_awaiter(std::coroutine_handle<>)
        : std::suspend_never()
    {}
};

template <typename T, typename FinalAwaiter>
class func_promise
{
public:
    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        return FinalAwaiter{ _continuation };
    }

    void set_continuation(std::coroutine_handle<> continuation) noexcept
    {
        _continuation = continuation;
    }

    func_template<T, FinalAwaiter> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<func_promise>;
        return func_template<T, FinalAwaiter>{ coroutine_handle::from_promise(*this) };
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


template<typename T, typename FinalAwaiter>
class func_template
{
    friend class co::thread;
public:
    using promise_type = func_promise<T, FinalAwaiter>;
    using value = T;

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

    explicit func_template(std::coroutine_handle<promise_type> coroutine)
        : _coroutine(coroutine)
    {}

    func_template(func_template&& t) noexcept
        : func_template(t._coroutine)
    {
        t._coroutine = nullptr;
    }

    func_template(const func_template&) = delete;
    func_template& operator=(const func_template&) = delete;
    func_template& operator=(func_template&& other) = delete;

    ~func_template()
    {
        if (_coroutine && !std::is_same_v<FinalAwaiter, impl::never_awaiter>)
            _coroutine.destroy();
    }

    auto operator co_await() const
    {
        return awaitable{ _coroutine };
    }

private:
    std::coroutine_handle<promise_type> _coroutine;
};

template <typename FinalAwaiter>
class func_promise<void, FinalAwaiter>
{
public:
    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        return FinalAwaiter{ _continuation };
    }

    void set_continuation(std::coroutine_handle<> continuation) noexcept
    {
        _continuation = continuation;
    }

    func_template<void, FinalAwaiter> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<func_promise<void, FinalAwaiter>>;
        return func_template<void, FinalAwaiter>{ coroutine_handle::from_promise(*this) };
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

using thread_func = impl::func_template<void, never_awaiter>;

} // namespace impl

template <typename T>
using func = impl::func_template<T, impl::symmetric_transfer_awaiter>;

}