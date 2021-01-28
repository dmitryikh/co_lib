#pragma once

#include <optional>
#include <experimental/coroutine>

namespace co::tmp
{

template <typename T>
class generator_shared_state 
{
public:
    void set_exception(std::exception_ptr exception_ptr)
    {
        _exception_ptr = exception_ptr;
        set_done();
    }

    void set_value(T value)
    {
        _value_opt = std::move(value);
    }

    std::optional<T> value()
    {
        if (is_done())
            return std::nullopt;
        return std::move(_value_opt).value();
    }

    void set_done()
    {
        _is_done = true;
    }

    bool is_done() const
    {
        return _is_done;
    }

private:
    bool _is_done = false;
    std::optional<T> _value_opt;
    std::exception_ptr _exception_ptr;
};

template <typename T>
class generator;

template <typename T>
class generator_promise
{
public:
    generator_promise() = default;

    generator<T> get_return_object() noexcept
    {
        using coroutine_handle = std::experimental::coroutine_handle<generator_promise<T>>;
        return { coroutine_handle::from_promise(*this), _state };
    }

    constexpr std::experimental::suspend_always initial_suspend() const noexcept { return {}; }
    constexpr std::experimental::suspend_always final_suspend() const noexcept { return {}; }

    void unhandled_exception()
    {
        _state.set_exception(std::current_exception());
    }

    void return_void()
    {
        _state.set_done();
    }

    std::experimental::suspend_always yield_value(T value) noexcept
    {
        _state.set_value(std::move(value));
        return {};
    }

    // Don't allow any use of 'co_await' inside the coroutine.
    template<typename U>
    std::experimental::suspend_never await_transform(U&& value) = delete;

private:
    generator_shared_state<T> _state;
};

template <typename T>
class generator
{
public:
    using promise_type = generator_promise<T>;

    generator(
        std::experimental::coroutine_handle<> coroutine,
        generator_shared_state<T>& state
    )
        : _coroutine(coroutine)
        , _state(state)
    {}

    generator(generator&& other) noexcept
        : generator(other._coroutine, other._state)
    {
        other._coroutine = nullptr;
    }

    ~generator()
    {
        if (_coroutine)
            _coroutine.destroy();
    }

    std::optional<T> next()
    {
        if (_state.is_done())
            return std::nullopt;

        _coroutine.resume();

        return _state.value();
    }

private:
    std::experimental::coroutine_handle<> _coroutine;
    generator_shared_state<T>& _state;
};

}