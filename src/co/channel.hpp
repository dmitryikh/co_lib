#pragma once

#include <chrono>
#include <mutex>

#include <boost/circular_buffer.hpp>
#include <co/impl/waiting_queue.hpp>
#include <co/status_codes.hpp>
#include <co/until.hpp>

namespace co::impl
{

struct dummy_mutex
{
    void lock(){};
    void unlock(){};
};

template <typename T, bool ThreadSafe>
struct channel_shared_state
{
public:
    using lock_type = std::conditional_t<ThreadSafe, std::mutex, dummy_mutex>;
    using queue_type = boost::circular_buffer<T>;

    explicit channel_shared_state(size_t capacity)
        : _queue(capacity)
    {}

    lock_type _mutex;
    bool _closed = false;
    queue_type _queue;
    impl::waiting_queue_base<ThreadSafe> _producer_waiting_queue;
    impl::waiting_queue_base<ThreadSafe> _consumer_waiting_queue;
};

}  // namespace co::impl

namespace co
{

/// \brief buffered channel to pass data of type T between co::threads
/// \tparam T type of data to be passed
///
/// channel owns a count referenced data inside. Thus it's cheap to copy a channel.
/// Usage:
/// \code
///     const size_t capacity = 2;
///     co::channel<int> ch(capacity);
///
///     auto th1 = co::thread(
///         [ch]() mutable -> co::func<void>
///         {
///           co_await ch.push(1).unwrap();
///           co_await ch.push(2).unwrap();
///           co_await ch.push(3).unwrap();
///           ch.close();
///         },
///         "producer");
///
///     while (true)
///     {
///         auto val = co_await ch.pop();
///         if (val == co::closed)
///             break;
///     }
///     co_await th1.join();
/// \endcode
template <typename T, bool ThreadSafe>
class channel_base
{
public:
    /// \brief create a new channel with capacity
    explicit channel_base(size_t capacity)
        : _state(std::make_shared<impl::channel_shared_state<T, ThreadSafe>>(capacity))
    {}

    /// \brief pushes a new value to the channel. Returns co::full if the channel is full
    /// \tparam T2 type
    /// \param t value of some type that can be convertible to channel's type T
    /// \return ok, co::full, co::close
    template <typename T2>
    co::result<void> try_push(T2&& t) requires(std::is_constructible_v<T, T2>);

    /// \brief pushes a new value to the channel. Blocks until the channel has space or is interrupted
    /// \param t value of some type that can be convertible to channel's type T
    /// \return ok, co::close, co::cancel, co::timeout
    template <typename T2>
    co::func<co::result<void>> push(T2&& t, co::until until = {}) requires(std::is_constructible_v<T, T2>);

    template <typename T2>
    co::result<void> blocking_push(T2&& t) requires(ThreadSafe || std::is_constructible_v<T, T2>);

    template <typename T2, typename Rep, typename Period>
    co::result<void> blocking_push(T2&& t,
                                   std::chrono::duration<Rep, Period> timeout) requires(ThreadSafe ||
                                                                                        std::is_constructible_v<T, T2>);

    /// \brief pop the front value from the channel or returns co::empty
    /// \return ok, co::empty, co::close
    co::result<T> try_pop();

    /// \brief pop the front value from the channel. Blocks until the channel has some values or is interrupted
    /// \return ok, co::close, co::cancel, co::timeout
    co::func<co::result<T>> pop(co::until until = {});

    /// \brief The blocking version of pop(). It will block the current OS thread until the channel has value or be
    /// closed. \return ok, co::close
    co::result<T> blocking_pop() requires(ThreadSafe);

    /// \brief The blocking version of pop(). It will block the current OS thread until the channel has
    /// value or be closed, or timeout has occured.
    /// \return ok, co::close, co::cancel, co::timeout
    template <typename Rep, typename Period>
    co::result<T> blocking_pop(std::chrono::duration<Rep, Period> timeout) requires(ThreadSafe);

    /// \brief closes the channel. Pop operations will return co::closed after drained last elements
    void close();

    /// \brief returns true if the channel is closed
    [[nodiscard]] bool is_closed() const;

private:
    void check_shared_state() const
    {
        if (_state == nullptr)
            throw std::runtime_error("channel shared state is nullptr");
    }

private:
    std::shared_ptr<impl::channel_shared_state<T, ThreadSafe>> _state;
};

template <typename T, bool ThreadSafe>
template <typename T2>
co::result<void> channel_base<T, ThreadSafe>::try_push(T2&& t) requires(std::is_constructible_v<T, T2>)
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    if (_state->_closed)
        return co::err(co::closed);

    if (_state->_queue.full())
        return co::err(co::full);

    _state->_queue.push_back(std::forward<T2>(t));
    _state->_consumer_waiting_queue.notify_one();
    return co::ok();
}

template <typename T, bool ThreadSafe>
template <typename T2>
co::func<co::result<void>> channel_base<T, ThreadSafe>::push(T2&& t,
                                                             co::until until) requires(std::is_constructible_v<T, T2>)
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    if (_state->_closed)
        co_return co::err(co::closed);

    while (_state->_queue.full() && !_state->_closed)
    {
        auto res = co_await _state->_producer_waiting_queue.wait(lk, until);
        if (res.is_err())
            co_return res.err();
    }

    if (_state->_closed)
        co_return co::err(co::closed);

    assert(!_state->_queue.full());
    _state->_queue.push_back(std::forward<T2>(t));
    _state->_consumer_waiting_queue.notify_one();
    co_return co::ok();
}

template <typename T, bool ThreadSafe>
template <typename T2>
co::result<void> channel_base<T, ThreadSafe>::blocking_push(T2&& t) requires(ThreadSafe ||
                                                                             std::is_constructible_v<T, T2>)
{
    return blocking_push(std::forward<T2>(t), std::chrono::steady_clock::duration::max());
}

template <typename T, bool ThreadSafe>
template <typename T2, typename Rep, typename Period>
co::result<void> channel_base<T, ThreadSafe>::blocking_push(
    T2&& t, std::chrono::duration<Rep, Period> timeout) requires(ThreadSafe || std::is_constructible_v<T, T2>)
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    if (_state->_closed)
        return co::err(co::closed);

    while (_state->_queue.full() && !_state->_closed)
    {
        // `blocking_wait` will reacquire the lock
        // TODO: timeout should be a global one, currently it is for every blocking_wait call here.
        co::result<void> res = co::ok();
        if (timeout == std::chrono::duration<Rep, Period>::max())
        {
            _state->_producer_waiting_queue.blocking_wait(lk);
        }
        else
        {
            res = _state->_producer_waiting_queue.blocking_wait(lk, timeout);
        }
        if (res.is_err())
            return res.err();
    }

    if (_state->_closed)
        return co::err(co::closed);

    assert(!_state->_queue.full());
    _state->_queue.push_back(std::forward<T2>(t));
    _state->_consumer_waiting_queue.notify_one();
    return co::ok();
}

template <typename T, bool ThreadSafe>
co::result<T> channel_base<T, ThreadSafe>::try_pop()
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    if (_state->_closed && _state->_queue.empty())
        return co::err(co::closed);

    if (_state->_queue.empty())
        return co::err(co::empty);

    result<T> res = co::ok(std::move(_state->_queue.front()));
    _state->_queue.pop_front();
    _state->_producer_waiting_queue.notify_one();
    return res;
}

template <typename T, bool ThreadSafe>
co::func<co::result<T>> channel_base<T, ThreadSafe>::pop(co::until until)
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    while (_state->_queue.empty() && !_state->_closed)
    {
        auto res = co_await _state->_consumer_waiting_queue.wait(lk, until);
        if (res.is_err())
        {
            // need to notify other consumer to wake up and try to get ready items
            _state->_consumer_waiting_queue.notify_one();
            co_return res.err();
        }
    }

    if (_state->_closed && _state->_queue.empty())
        co_return co::err(co::closed);

    assert(!_state->_queue.empty());
    result<T> res = co::ok(std::move(_state->_queue.front()));
    _state->_queue.pop_front();
    _state->_producer_waiting_queue.notify_one();
    co_return res;
}

template <typename T, bool ThreadSafe>
co::result<T> channel_base<T, ThreadSafe>::blocking_pop() requires(ThreadSafe)
{
    return blocking_pop(std::chrono::steady_clock::duration::max());
}

template <typename T, bool ThreadSafe>
template <typename Rep, typename Period>
co::result<T> channel_base<T, ThreadSafe>::blocking_pop(std::chrono::duration<Rep, Period> timeout) requires(ThreadSafe)
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    while (_state->_queue.empty() && !_state->_closed)
    {
        co::result<void> res = co::ok();
        if (timeout == std::chrono::duration<Rep, Period>::max())
        {
            _state->_consumer_waiting_queue.blocking_wait(lk);
        }
        else
        {
            res = _state->_consumer_waiting_queue.blocking_wait(lk, timeout);
        }
        if (res.is_err())
        {
            // need to notify other consumer to wake up and try to get ready items
            _state->_consumer_waiting_queue.notify_one();
            return res.err();
        }
    }

    if (_state->_closed && _state->_queue.empty())
        return co::err(co::closed);

    assert(!_state->_queue.empty());
    result<T> res = co::ok(std::move(_state->_queue.front()));
    _state->_queue.pop_front();
    _state->_producer_waiting_queue.notify_one();
    return res;
}

template <typename T, bool ThreadSafe>
void channel_base<T, ThreadSafe>::close()
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    _state->_closed = true;
    _state->_producer_waiting_queue.notify_all();
    _state->_consumer_waiting_queue.notify_all();
}

template <typename T, bool ThreadSafe>
[[nodiscard]] bool channel_base<T, ThreadSafe>::is_closed() const
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    return _state->_closed;
}

template <typename T>
using channel = channel_base<T, /*ThreadSafe=*/false>;
template <typename T>
using ts_channel = channel_base<T, /*ThreadSafe=*/true>;

}  // namespace co