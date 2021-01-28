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

    void set_exception(std::exception_ptr excPtr)
    {
        _variant = excPtr;
    }

    void set_value(T value)
    {
        _variant = std::move(value);
    }

    bool is_done() const
    {
        return _variant.index() != 0;
    }

    T value()
    {
        if (T* valuePtr = std::get_if<T>(&_variant); valuePtr != nullptr)
            return std::move(*valuePtr);
        else if (auto excPtr = std::get_if<std::exception_ptr>(&_variant); excPtr != nullptr)
            std::rethrow_exception(*excPtr);
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

    void set_exception(std::exception_ptr excPtr)
    {
        _is_done = true;
        _exception_ptr = excPtr;
    }

    void set_value()
    {
        _is_done = true;
    }

    bool is_done() const
    {
        return _is_done;
    }

    void value()
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