#pragma once 

#include <co/std.hpp>
#include <co/impl/shared_state.hpp>

namespace co::tmp
{

using co::impl::shared_state;

template <typename T>
class eager;

template <typename T>
class eager_promise
{
public:
    eager_promise() = default;

    eager<T> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<eager_promise<T>>;
        return { coroutine_handle::from_promise(*this), _state };
    }

    constexpr std::suspend_never initial_suspend() const noexcept { return {}; }
    constexpr std::suspend_always final_suspend() const noexcept { return {}; }

    void unhandled_exception()
    {
        _state.set_exception(std::current_exception());
    }

    void return_value(T value)
    {
        _state.set_value(std::move(value));
    }

    // Don't allow any use of 'co_await' inside the coroutine.
    template<typename U>
    std::suspend_never await_transform(U&& value) = delete;

private:
    shared_state<T> _state;
};

template <typename T>
class eager
{
public:
    using promise_type = eager_promise<T>;

    eager(
        std::coroutine_handle<> coroutine,
        shared_state<T>& state
    )
        : _coroutine(coroutine)
        , _state(state)
    {}

    eager(eager&& other) noexcept
        : eager(other._coroutine, other._state)
    {
        other._coroutine = nullptr;
    }

    ~eager()
    {
        if (_coroutine)
            _coroutine.destroy();
    }

    T value()
    {
        return _state.value();
    }

private:
    std::coroutine_handle<> _coroutine;
    shared_state<T>& _state;
};

template <>
class eager_promise<void>
{
public:
    eager_promise() = default;

    eager<void> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<eager_promise<void>>;
        return { coroutine_handle::from_promise(*this), _state };
    }

    constexpr std::suspend_never initial_suspend() const noexcept { return {}; }
    constexpr std::suspend_always final_suspend() const noexcept { return {}; }

    void unhandled_exception()
    {
        _state.set_exception(std::current_exception());
    }

    void return_void()
    {
        _state.set_value();
    }

    template<typename U>
    std::suspend_never await_transform(U&& value) = delete;

private:
    shared_state<void> _state;
};

}