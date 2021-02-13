#pragma once

#include <cassert>
#include <type_traits>
#include <co/impl/shared_state.hpp>
#include <co/result.hpp>
#include <co/std.hpp>

namespace co
{

template <typename T>
class func;

namespace impl
{

// NOTE: we don't need to change thread_storage_ptr here because we don't change
// the active co::thread. We are just returning control to the parent frame
// NOTE: the frame will be destroyed in func's destructor
class symmetric_transfer_awaiter
{
public:
    symmetric_transfer_awaiter(std::coroutine_handle<> continuation)
        : _continuation(continuation)
    {}

    bool await_ready() const noexcept
    {
        return false;
    }

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

template <typename Promise>
class other_func_awaiter
{
    using type = typename Promise::type;

public:
    other_func_awaiter(std::coroutine_handle<Promise> coroutine) noexcept
        : _coroutine(coroutine)
    {}

    bool await_ready() const noexcept
    {
        return !_coroutine || _coroutine.done();
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept
    {
        _coroutine.promise().set_continuation(continuation);
        return _coroutine;
    }

    type await_resume() requires(!std::is_same_v<type, void>)
    {
        return std::move(_coroutine.promise().state().value());
    }

    void await_resume() requires(std::is_same_v<type, void>)
    {
        _coroutine.promise().state().value();
    }

public:
    std::coroutine_handle<Promise> _coroutine;
};

template <typename T>
class func_promise_base
{
public:
    using type = T;

    std::suspend_always initial_suspend() noexcept
    {
        return {};
    }

    auto final_suspend() noexcept
    {
        return symmetric_transfer_awaiter{ _continuation };
    }

    void set_continuation(std::coroutine_handle<> continuation) noexcept
    {
        _continuation = continuation;
    }

    void unhandled_exception()
    {
        _state.set_exception(std::current_exception());
    }

    shared_state<T>& state()
    {
        return _state;
    }

protected:
    std::coroutine_handle<> _continuation;
    shared_state<T> _state;
};

template <typename T>
class func_promise : public func_promise_base<T>
{
public:
    auto get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<func_promise>;
        return func<T>{ coroutine_handle::from_promise(*this) };
    }

    void return_value(T value)
    {
        this->_state.set_value(std::move(value));
    }
};

}  // namespace impl

template <typename T>
co::func<typename T::ok_type> unwrap(co::func<T> orig) requires(co::is_result_v<T> &&
                                                                !std::is_void_v<typename T::ok_type>)
{
    auto res = co_await orig;
    co_return std::move(res.unwrap());
}

template <typename T>
co::func<typename T::ok_type> unwrap(co::func<T> orig) requires(
    co::is_result_v<T>&& std::is_void_v<typename T::ok_type>)
{
    auto res = co_await orig;
    res.unwrap();
}

template <typename T>
class [[nodiscard("co_await me")]] func
{
public:
    using promise_type = impl::func_promise<T>;
    using value = T;

public:
    explicit func(std::coroutine_handle<promise_type> coroutine)
        : _coroutine(coroutine)
    {}

    func(func&& t) noexcept
        : func(t._coroutine)
    {
        t._coroutine = nullptr;
    }

    func(const func&) = delete;
    func& operator=(const func&) = delete;
    func& operator=(func&& other) = delete;

    ~func()
    {
        if (!_coroutine)
            return;

        assert(_coroutine.done());
        _coroutine.destroy();
    }

    auto operator co_await() const
    {
        return impl::other_func_awaiter<promise_type>{ _coroutine };
    }

    auto unwrap()
    {
        return co::unwrap<T>(std::move(*this));
    }

private:
    std::coroutine_handle<promise_type> _coroutine;
};

namespace impl
{

template <>
class func_promise<void> : public func_promise_base<void>
{
public:
    auto get_return_object() noexcept
    {
        using coroutine_handle = std::coroutine_handle<func_promise>;
        return func<void>{ coroutine_handle::from_promise(*this) };
    }

    void return_void()
    {
        this->_state.set_value();
    }
};

template <typename T>
struct is_func : std::false_type
{};

template <typename T>
struct is_func<func<T>> : std::true_type
{};

}  // namespace impl

template <typename T>
inline constexpr bool is_func_v = impl::is_func<T>::value;

template <typename T>
concept FuncConcept = is_func_v<T>;

template <typename F>
concept FuncLambdaConcept = requires(F a)
{
    {
        a()
    }
    ->FuncConcept;
};

template <FuncLambdaConcept F, typename... Args>
auto invoke(F&& f, Args&&... args)
{
    return [](F f, Args&&... args) -> std::invoke_result_t<F, Args...> {
        co_return co_await f(std::forward<Args>(args)...);
    }(std::move(f), std::forward<Args>(args)...);
}

}  // namespace co
