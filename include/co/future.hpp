#pragma once

#include <co/condition_variable.hpp>
#include <co/func.hpp>
#include <co/impl/shared_state.hpp>
#include <co/result.hpp>
#include <co/until.hpp>

namespace co
{

namespace impl
{

template <typename T>
class future_shared_state : public shared_state<T>
{
    using base = shared_state<T>;

public:
    void promise_destroyed() requires(co::is_result_v<T>)
    {
        if (!base::is_done())
        {
            // promise has been destroyed but it didn't provide a result
            base::set_value(co::err(co::broken));
            _cv.notify_all();
        }
    }

    void promise_destroyed() requires(!co::is_result_v<T>)
    {
        if (!base::is_done())
        {
            // promise has been destroyed but it didn't provide a result
            base::set_exception(
                std::make_exception_ptr(co::exception(co::broken, "promise destroyed whithout result")));
            _cv.notify_all();
        }
    }

    void set_exception(std::exception_ptr exc_ptr)
    {
        if (base::is_done())
            throw co::exception(co::other, "promise already set");

        base::set_exception(std::move(exc_ptr));
        _cv.notify_all();
    }

    template <typename... Args>
    void set_value(Args&&... args)
    {
        if (base::is_done())
            throw co::exception(co::other, "promise already set");

        base::set_value(std::forward<Args>(args)...);
        _cv.notify_all();
    }

    co::func<void> wait()
    {
        co_await _cv.wait([this]() { return base::is_done(); });
    }

    co::func<result<void>> wait(co::until until)
    {
        co_return co_await _cv.wait([this]() { return base::is_done(); }, until);
    }

    co::func<T> get()
    {
        co_await wait();
        co_return std::move(base::value());
    }

    co::func<result<T>> get(co::until until)
    {
        auto res = co_await wait(until);
        if (res.is_err())
            co_return res.err();

        co_return co::ok(std::move(base::value()));
    }

private:
    co::condition_variable _cv;
};

template <typename T>
using future_shared_state_sp = std::shared_ptr<future_shared_state<T>>;

}  // namespace impl

template <typename T>
class promise;

template <typename T>
class future
{
    friend class promise<T>;

private:
    future(impl::future_shared_state_sp<T> st)
        : _shared_state{ std::move(st) }
    {}

public:
    future() = default;

    bool valid() const noexcept
    {
        return _shared_state != nullptr;
    }

    co::func<T> get()
    {
        check_shared_state();
        co_return co_await _shared_state->get();
    }

    co::func<co::result<T>> get(co::until until)
    {
        check_shared_state();
        co_return co_await _shared_state->get(until);
    }

private:
    void check_shared_state() const noexcept(false)
    {
        if (!_shared_state)
            throw co::exception(co::broken, "future is uninitialized");
    }

private:
    impl::future_shared_state_sp<T> _shared_state;
};

template <typename T>
class promise
{
public:
    promise()
        : _shared_state{ std::make_shared<impl::future_shared_state<T>>() }
    {}

    promise(const promise&) = delete;
    promise& operator=(const promise&) = delete;
    promise(promise&&) = default;
    promise& operator=(promise&&) = default;

    future<T> get_future() const
    {
        check_shared_state();
        return future<T>{ _shared_state };
    }

    void set_exception(std::exception_ptr exc_ptr)
    {
        check_shared_state();
        _shared_state->set_exception(std::move(exc_ptr));
    }

    template <typename... Args>
    void set_value(Args&&... args) requires(std::is_constructible_v<T, Args...>)
    {
        check_shared_state();
        _shared_state->set_value(std::forward<Args>(args)...);
    }

    ~promise()
    {
        if (_shared_state)
            _shared_state->promise_destroyed();
    }

private:
    void check_shared_state() const noexcept(false)
    {
        if (!_shared_state)
            throw co::exception(co::broken, "promise is uninitialized");
    }

private:
    impl::future_shared_state_sp<T> _shared_state;
};

}  // namespace co