#pragma once

#include <co/impl/waiting_queue.hpp>

namespace co
{

class condition_variable
{
public:
    /// set of wait methods without a mutex
    func<void> wait()
    {
        co_await _waiting_queue.wait();
    }

    template <typename Predicate>
    func<void> wait(Predicate predicate)
    {
        while (!predicate())
        {
            co_await wait();
        }
    }

    template <class Rep, class Period>
    func<result<void>> wait_for(std::chrono::duration<Rep, Period> sleep_duration,
                                const stop_token& token = impl::dummy_stop_token)
    {
        co_return co_await _waiting_queue.wait_for(sleep_duration, token);
    }

    template <class Rep, class Period, typename Predicate>
    func<result<void>> wait_for(std::chrono::duration<Rep, Period> sleep_duration,
                                Predicate predicate,
                                const stop_token& token = impl::dummy_stop_token)
    {
        return wait_until(std::chrono::steady_clock::now() + sleep_duration, std::move(predicate), token);
    }

    template <class Clock, class Duration>
    func<result<void>> wait_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = {})
    {
        co_return co_await _waiting_queue.wait_until(sleep_time, token);
    }

    template <class Clock, class Duration, typename Predicate>
    func<result<void>> wait_until(std::chrono::time_point<Clock, Duration> sleep_time,
                                  Predicate predicate,
                                  const co::stop_token& token = impl::dummy_stop_token)
    {
        while (!predicate())
        {
            auto res = co_await wait_until(sleep_time, token);
            if (res == co::timeout)
            {
                if (predicate())
                    co_return co::ok();
                else
                    co_return co::err(co::timeout);
            }
            else if (res.is_err())
            {
                co_return res.err();
            }
        }
        co_return co::ok();
    }

    void notify_one()
    {
        _waiting_queue.notify_one();
    }

    void notify_all()
    {
        _waiting_queue.notify_all();
    }

private:
    co::impl::waiting_queue _waiting_queue;
};

}  // namespace co