#pragma once

#include <co/channel.hpp>
#include <co/event.hpp>
#include <co/exception.hpp>
#include <co/error_code.hpp>
#include <co/func.hpp>

namespace co
{

/// @brief one_shot is a channel to pass zero or one value between two peers
template <typename T>
class one_shot
{
public:
    one_shot() = default;

    result<T> try_pop()
    {
        if (_closed)
            return co::err(co::closed);

        if (!_event.is_notified())
            return co::err(co::empty);

        return pop_actual();
    }

    co::func<result<T>> pop(const co::stop_token& token = impl::dummy_stop_token)
    {
        if (_closed)
            co_return co::err(co::closed);

        if (_event.is_notified())
            co_return co::ok(pop_actual());

        auto res = co_await _event.wait(token);
        if (res.is_err())
            co_return res.err();
        assert(_event.is_notified());

        if (_closed)
            co_return co::err(co::closed);

        co_return co::ok(pop_actual());
    }

    template <class Rep, class Period>
    co::func<result<T>> pop_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token = impl::dummy_stop_token)
    {
        if (_closed)
            co_return co::err(co::closed);

        if (_event.is_notified())
            co_return co::ok(pop_actual());

        auto res = co_await _event.wait_for(sleep_duration, token);
        if (res.is_err())
            co_return res.err();
        assert(_event.is_notified());

        if (_closed)
            co_return co::err(co::closed);

        co_return co::ok(pop_actual());
    }

    template <class Clock, class Duration>
    co::func<result<T>> pop_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = impl::dummy_stop_token)
    {
        return pop_for(sleep_time - Clock::now(), token);
    }

    template <typename T2>
    result<void> push(T2&& t)
    {
        if (_closed)
            return co::err(co::closed);

        if (_event.is_notified())
            throw co::exception(co::other, "one_shot pushed twice");

        assert(!_opt.has_value());

        _opt = T{ std::forward<T2>(t) };
        _event.notify();

        return co::ok();
    }

    void close()
    {
        _closed = true;
        _event.notify();
    }

private:
    T pop_actual()
    {
        std::cout << "pop_actual\n";
        if (!_opt.has_value())
            throw co::exception(co::other, "one_shot popped twice");

        T tmp = std::move(_opt.value());
        _opt = std::nullopt;
        return tmp;
    }

private:
    bool _closed = false;
    co::event _event;
    std::optional<T> _opt;
};

}  // namespace co