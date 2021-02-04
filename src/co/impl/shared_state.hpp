#pragma once

#include <variant>

namespace co::impl
{

template <typename T>
class shared_state
{
    using Variant = std::variant<std::monostate, T, std::exception_ptr>;
public:
    shared_state() = default;

    void set_exception(std::exception_ptr exc_ptr)
    {
        _variant = exc_ptr;
    }

    template <typename... Args>
    void set_value(Args&&... args) requires (std::is_constructible_v<T, Args...>)
    {
        _variant.template emplace<T>(std::forward<Args>(args)...);
    }

    bool is_done() const
    {
        return _variant.index() != 0;
    }

    T& value()
    {
        if (T* valuePtr = std::get_if<T>(&_variant); valuePtr != nullptr)
            return *valuePtr;
        else if (auto exc_ptr = std::get_if<std::exception_ptr>(&_variant); exc_ptr != nullptr)
            std::rethrow_exception(*exc_ptr);
        else
            throw std::runtime_error("state is not set");
    }

    const T& value() const
    {
        if (T* valuePtr = std::get_if<T>(&_variant); valuePtr != nullptr)
            return *valuePtr;
        else if (auto exc_ptr = std::get_if<std::exception_ptr>(&_variant); exc_ptr != nullptr)
            std::rethrow_exception(*exc_ptr);
        else
            throw std::runtime_error("state is not set");
    }

private:
    Variant _variant;
};

template <>
class shared_state<void>
{
public:
    shared_state() = default;

    void set_exception(std::exception_ptr exc_ptr)
    {
        _is_done = true;
        _exception_ptr = exc_ptr;
    }

    void set_value()
    {
        _is_done = true;
    }

    bool is_done() const
    {
        return _is_done;
    }

    void value() const
    {
        if (!is_done())
            throw std::runtime_error("state is not set");

        if (_exception_ptr)
            std::rethrow_exception(_exception_ptr);
    }

private:
    bool _is_done = false;
    std::exception_ptr _exception_ptr;
};

}