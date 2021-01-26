#pragma once 

#include <iostream>
#include <variant>
#include <experimental/coroutine>

template <typename T>
class instant;

template <typename T>
class instant_promise
{
    using Variant = std::variant<std::monostate, T, std::exception_ptr>;
public:
    instant_promise() = default;

    ~instant_promise()
    {
        std::cout << "~instant_promise()\n";
    }

    instant<T> get_return_object() noexcept
    {
        using coroutine_handle = std::experimental::coroutine_handle<instant_promise<T>>;
        return instant<T>{ coroutine_handle::from_promise(*this) };
    }

    constexpr std::experimental::suspend_never initial_suspend() const noexcept { return {}; }
    constexpr std::experimental::suspend_always final_suspend() const noexcept { return {}; }

    void unhandled_exception()
    {
        _variant = std::current_exception();
    }

    void return_value(T value)
    {
        std::cout << "return_value(T value)" << std::endl;
        _variant = value;
    }

    T value()
    {
        if (T* valuePtr = std::get_if<T>(&_variant); valuePtr != nullptr)
            return std::move(*valuePtr);
        else if (auto excPtr = std::get_if<std::exception_ptr>(&_variant); excPtr != nullptr)
            std::rethrow_exception(*excPtr);
        else
            throw std::runtime_error("state undefined");
    }

    // Don't allow any use of 'co_await' inside the coroutine.
    template<typename U>
    std::experimental::suspend_never await_transform(U&& value) = delete;

private:
    Variant _variant;
};

template <typename T>
class instant
{
public:
    using promise_type = instant_promise<T>;

    instant() = delete;

    instant(instant&& other) noexcept
        : _coroutine(other._coroutine)
    {
        other._coroutine = nullptr;
    }

    ~instant()
    {
        if (_coroutine)
        {
            std::cout << "~instant destroy" << std::endl;
            _coroutine.destroy();
        }
    }

    T value()
    {
        return _coroutine.promise().value();
    }

private:
    friend class instant_promise<T>;

    explicit instant(std::experimental::coroutine_handle<promise_type> coroutine) noexcept
        : _coroutine(coroutine)
    {}

private:
    std::experimental::coroutine_handle<promise_type> _coroutine;
};

template <>
class instant_promise<void>
{
public:
    instant_promise() = default;

    ~instant_promise()
    {
        std::cout << "~instant_promise()\n";
    }

    instant<void> get_return_object() noexcept
    {
        using coroutine_handle = std::experimental::coroutine_handle<instant_promise<void>>;
        return instant<void>{ coroutine_handle::from_promise(*this) };
    }

    constexpr std::experimental::suspend_never initial_suspend() const noexcept { return {}; }
    constexpr std::experimental::suspend_always final_suspend() const noexcept { return {}; }

    void unhandled_exception()
    {
        _exception_ptr = std::current_exception();
    }

    void return_void() {}

    void value()
    {
        if (_exception_ptr)
            std::rethrow_exception(_exception_ptr);
    }

    template<typename U>
    std::experimental::suspend_never await_transform(U&& value) = delete;

private:
    std::exception_ptr _exception_ptr;
};
