#pragma once

#include <co/impl/shared_state.hpp>
#include <co/condition_variable.hpp>
#include <co/result.hpp>
#include <co/func.hpp>

namespace co
{

namespace impl
{

template <typename T>
class future_shared_state : public shared_state<T>
{
    using base = shared_state<T>;
public:
    void promise_destroyed() requires (co::is_result_v<T>)
    {
        if (!base::is_done())
        {
            // promise has been destroyed but it didn't provide a result
            base::set_value(co::err(co::broken));
            _cv.notify_all();
        }
    }

    void promise_destroyed() requires (!co::is_result_v<T>)
    {
        if (!base::is_done())
        {
            // promise has been destroyed but it didn't provide a result
            base::set_exception(
                std::make_exception_ptr(
                    co::exception(co::broken, "promise destroyed whithout result")
                )
            );
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

    co::func<result<void>> wait(const stop_token& token)
    {
        co_return co_await _cv.wait([this]() { return base::is_done(); }, token);
    }

    template <class Rep, class Period>
    co::func<result<void>> wait_for(std::chrono::duration<Rep, Period> sleep_duration, const stop_token& token = impl::dummy_stop_token)
    {
        co_return co_await _cv.wait_for(
            sleep_duration,
            [this]() { return base::is_done(); },
            token
        );
    }

    template <class Clock, class Duration>
    co::func<result<void>> wait_until(std::chrono::time_point<Clock, Duration> sleep_time, const stop_token& token = impl::dummy_stop_token)
    {
        co_return co_await _cv.wait_until(
            sleep_time,
            [this]() { return base::is_done(); },
            token
        );
    }

    co::func<T> get()
    {
        co_await wait();
        co_return std::move(base::value());
    }

    template <class Rep, class Period>
    co::func<result<T>> get_for(
        std::chrono::duration<Rep, Period> sleep_duration,
        const stop_token& token = impl::dummy_stop_token
    ) requires (!co::is_result_v<T>)
    {
        auto res = co_await wait_for(sleep_duration, token);
        if (res.is_err())
            co_return res.err();

        co_return co::ok(std::move(base::value()));
    }

    template <class Rep, class Period>
    co::func<T> get_for(
        std::chrono::duration<Rep, Period> sleep_duration,
        const stop_token& token = impl::dummy_stop_token
    ) requires (co::is_result_v<T>)
    {
        auto res = co_await wait_for(sleep_duration, token);
        if (res.is_err())
            co_return res.err();

        co_return std::move(base::value());
    }

    template <class Clock, class Duration>
    co::func<result<T>> get_until(
        std::chrono::time_point<Clock, Duration> sleep_time,
        const stop_token& token = impl::dummy_stop_token
    ) requires (!co::is_result_v<T>)
    {
        auto res = co_await wait_until(sleep_time, token);
        if (res.is_err())
            co_return res.err();

        co_return co::ok(std::move(base::value()));
    }

    template <class Clock, class Duration>
    co::func<T> get_until(
        std::chrono::time_point<Clock, Duration> sleep_time,
        const stop_token& token = impl::dummy_stop_token
    ) requires (co::is_result_v<T>)
    {
        auto res = co_await wait_until(sleep_time, token);
        if (res.is_err())
            co_return res.err();

        co_return std::move(base::value());
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

    template <class Rep, class Period>
    co::func<T> get_for(
        std::chrono::duration<Rep, Period> sleep_duration,
        const stop_token& token = impl::dummy_stop_token
    ) requires (co::is_result_v<T>)
    {
        check_shared_state();
        co_return co_await _shared_state->get_for(sleep_duration, token);
    }

    template <class Clock, class Duration>
    co::func<T> get_until(
        std::chrono::time_point<Clock, Duration> sleep_time,
        const stop_token& token = impl::dummy_stop_token
    ) requires (co::is_result_v<T>)
    {
        check_shared_state();
        co_return co_await _shared_state->get_until(sleep_time, token);
    }

    // TODO: add all get_for, get_until

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
    void set_value(Args&&... args) requires (std::is_constructible_v<T, Args...>)
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