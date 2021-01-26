#pragma once 

#include <iostream>
#include <variant>
#include <experimental/coroutine>

template <typename T>
class lazy;

template <typename T>
class lazy_promise
{
    using Variant = std::variant<std::monostate, T, std::exception_ptr>;
public:
    lazy_promise() = default;

    ~lazy_promise()
    {
        std::cout << "~lazy_promise()\n";
    }

    lazy<T> get_return_object() noexcept
    {
        using coroutine_handle = std::experimental::coroutine_handle<lazy_promise<T>>;
        return lazy<T>{ coroutine_handle::from_promise(*this) };
    }

    constexpr std::experimental::suspend_always initial_suspend() const noexcept { return {}; }
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
        if (_variant.index() != 0)
        {
            // holds something already, seems like value() called twice
            throw std::runtime_error("value() called twice");
        }

        using coroutine_handle = std::experimental::coroutine_handle<lazy_promise<T>>;
        auto handle = coroutine_handle::from_promise(*this);
        handle.resume();

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
class lazy
{
public:
    using promise_type = lazy_promise<T>;

    lazy() = delete;

    lazy(lazy&& other) noexcept
        : _coroutine(other._coroutine)
    {
        other._coroutine = nullptr;
    }

    ~lazy()
    {
        if (_coroutine)
        {
            std::cout << "~lazy destroy" << std::endl;
            _coroutine.destroy();
        }
    }

    T value()
    {
        return _coroutine.promise().value();
    }

private:
    friend class lazy_promise<T>;

    explicit lazy(std::experimental::coroutine_handle<promise_type> coroutine) noexcept
        : _coroutine(coroutine)
    {}

private:
    std::experimental::coroutine_handle<promise_type> _coroutine;
};

template <>
class lazy_promise<void>
{
public:
    lazy_promise() = default;

    ~lazy_promise()
    {
        std::cout << "~lazy_promise()\n";
    }

    lazy<void> get_return_object() noexcept
    {
        using coroutine_handle = std::experimental::coroutine_handle<lazy_promise<void>>;
        return lazy<void>{ coroutine_handle::from_promise(*this) };
    }

    constexpr std::experimental::suspend_always initial_suspend() const noexcept { return {}; }
    constexpr std::experimental::suspend_always final_suspend() const noexcept { return {}; }

    void unhandled_exception()
    {
        _exception_ptr = std::current_exception();
    }

    void return_void() {}

    void value()
    {
        if (resumed)
        {
            // holds something already, seems like value() called twice
            throw std::runtime_error("value() called twice");
        }
        resumed = true;

        using coroutine_handle = std::experimental::coroutine_handle<lazy_promise<void>>;
        auto handle = coroutine_handle::from_promise(*this);
        handle.resume();

        if (_exception_ptr)
            std::rethrow_exception(_exception_ptr);
    }

    template<typename U>
    std::experimental::suspend_never await_transform(U&& value) = delete;

private:
    bool resumed = false;
    std::exception_ptr _exception_ptr;
};