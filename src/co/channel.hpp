#pragma once

#include <boost/circular_buffer.hpp>
#include <co/event.hpp>
#include <co/impl/waiting_queue.hpp>

namespace co::impl
{

enum class channel_error_code
{
    empty = 1,
    full = 2,
    closed = 3
};

struct channel_error_code_category : std::error_category
{
    const char* name() const noexcept override
    {
        return "co_channel errors";
    }

    std::string message(int ev) const override
    {
        switch (static_cast<channel_error_code>(ev))
        {
        case channel_error_code::empty:
            return "empty";
        case channel_error_code::full:
            return "full";
        case channel_error_code::closed:
            return "closed";
        }
        assert(false);
        return "undefined";
    }
};

const channel_error_code_category global_channel_error_code_category{};

std::error_code make_error_code(channel_error_code e)
{
    return std::error_code{static_cast<int>(e), global_channel_error_code_category};
}

}  // namespace co::impl

namespace std {
    template <> struct is_error_code_enum<co::impl::channel_error_code> : true_type {};
}

namespace co
{

const auto full = make_error_code(impl::channel_error_code::full);
const auto empty = make_error_code(impl::channel_error_code::empty);
const auto closed = make_error_code(impl::channel_error_code::closed);


template <typename T>
class channel
{
    using queue_type = boost::circular_buffer<T>;
public:

    channel(size_t capacity)
        : _queue(capacity)
    {}

    template <typename T2>
    result<void> try_push(T2&& t)
    {
        if (_closed)
            return co::err(co::closed);

        if (_queue.full())
            return co::err(co::full);

        _queue.push_back(std::forward<T2>(t));
        _consumer_waiting_queue.notify_one();
        return co::ok();
    }

    template <typename T2>
    func<result<void>> push(T2&& t, const co::stop_token& token = impl::dummy_stop_token)
    {
        if (_closed)
            co_return co::err(co::closed);

        while (_queue.full() && !_closed)
        {
            auto res = co_await _producer_waiting_queue.wait(token);
            if (res.is_err())
                co_return res.err();
        }

        if (_closed)
            co_return co::err(co::closed);

        assert(!_queue.full());
        _queue.push_back(std::forward<T2>(t));
        _consumer_waiting_queue.notify_one();
        co_return co::ok();
    }

    template <typename T2, class Rep, class Period>
    func<result<void>> push_for(T2&& t, std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token = impl::dummy_stop_token)
    {
        return push_until(std::forward<T2>(t), std::chrono::steady_clock::now() + sleep_duration, token);
    }

    template <typename T2, class Clock, class Duration>
    func<result<void>> push_until(T2&& t, std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = impl::dummy_stop_token)
    {
        if (_closed)
            co_return co::err(co::closed);

        while (_queue.full() && !_closed)
        {
            auto res = co_await _producer_waiting_queue.wait_until(sleep_time, token);
            if (res.is_err())
                co_return res.err();
        }

        if (_closed)
            co_return co::err(co::closed);

        assert(!_queue.full());
        _queue.push_back(std::forward<T2>(t));
        _consumer_waiting_queue.notify_one();
        co_return co::ok();
    }

    result<T> try_pop()
    {
        if (_closed && _queue.empty())
            return co::err(co::closed);

        if (_queue.empty())
            return co::err(co::empty);

        result<T> res = co::ok(std::move(_queue.front()));
        _queue.pop_front();
        _producer_waiting_queue.notify_one();
        return res;
    }

    func<result<T>> pop(const co::stop_token& token = impl::dummy_stop_token)
    {
        while (_queue.empty() && !_closed)
        {
            auto res = co_await _consumer_waiting_queue.wait(token);
            if (res.is_err())
            {
                _consumer_waiting_queue.notify_one();
                co_return res.err();
            }
        }

        if (_closed && _queue.empty())
            co_return co::err(co::closed);

        assert(!_queue.empty());
        result<T> res = co::ok(std::move(_queue.front()));
        _queue.pop_front();
        _producer_waiting_queue.notify_one();
        co_return res;
    }

    template <class Rep, class Period>
    func<result<T>> pop_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token = impl::dummy_stop_token)
    {
        return pop_until(std::chrono::steady_clock::now() + sleep_duration, token);
    }

    template <class Clock, class Duration>
    func<result<T>> pop_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = impl::dummy_stop_token)
    {
        while (_queue.empty() && !_closed)
        {
            auto res = co_await _consumer_waiting_queue.wait_until(sleep_time, token);
            if (res.is_err())
            {
                _consumer_waiting_queue.notify_one();
                co_return res.err();
            }
        }

        if (_closed && _queue.empty())
            co_return co::err(co::closed);

        assert(!_queue.empty());
        result<T> res = co::ok(std::move(_queue.front()));
        _queue.pop_front();
        _producer_waiting_queue.notify_one();
        co_return res;
    }

    void close()
    {
        _closed = true;
        _producer_waiting_queue.notify_all();
        _consumer_waiting_queue.notify_all();
    }

    bool is_closed() const
    {
        return _closed;
    }

private:
    bool _closed = false;
    queue_type _queue;
    impl::waiting_queue _producer_waiting_queue;
    impl::waiting_queue _consumer_waiting_queue;
};

} // namespace co