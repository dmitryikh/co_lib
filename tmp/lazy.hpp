#pragma once

#include <co/impl/shared_state.hpp>
#include <co/std.hpp>

namespace co::tmp
{

using co::impl::shared_state;

template <typename T>
class lazy;

template <typename T>
class lazy_promise
{
public:
    lazy_promise() = default;

    lazy<T> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<lazy_promise<T>>;
        return { coroutine_handle::from_promise(*this), _state };
    }

    constexpr std::suspend_always initial_suspend() const noexcept
    {
        return {};
    }
    constexpr std::suspend_always final_suspend() const noexcept
    {
        return {};
    }

    void unhandled_exception()
    {
        _state.set_exception(std::current_exception());
    }

    void return_value(T value)
    {
        _state.set_value(std::move(value));
    }

    // Don't allow any use of 'co_await' inside the coroutine.
    template <typename U>
    std::suspend_never await_transform(U&& value) = delete;

private:
    shared_state<T> _state;
};

template <typename T>
class lazy
{
public:
    using promise_type = lazy_promise<T>;

    lazy(std::coroutine_handle<> coroutine, shared_state<T>& state)
        : _coroutine(coroutine)
        , _state(state)
    {}

    lazy(lazy&& other) noexcept
        : lazy(other._coroutine, other._state)
    {
        other._coroutine = nullptr;
    }

    ~lazy()
    {
        if (_coroutine)
            _coroutine.destroy();
    }

    T value()
    {
        if (_state.is_done())
            throw std::runtime_error("value() called > 1 times");
        _coroutine.resume();
        return _state.value();
    }

private:
    std::coroutine_handle<> _coroutine;
    shared_state<T>& _state;
};

template <>
class lazy_promise<void>
{
public:
    lazy_promise() = default;

    lazy<void> get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<lazy_promise<void>>;
        return { coroutine_handle::from_promise(*this), _state };
    }

    constexpr std::suspend_always initial_suspend() const noexcept
    {
        return {};
    }
    constexpr std::suspend_always final_suspend() const noexcept
    {
        return {};
    }

    void unhandled_exception()
    {
        _state.set_exception(std::current_exception());
    }

    void return_void()
    {
        _state.set_value();
    }

    template <typename U>
    std::suspend_never await_transform(U&& value) = delete;

private:
    shared_state<void> _state;
};

}  // namespace co::tmp