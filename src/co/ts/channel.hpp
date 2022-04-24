#pragma once

#include <boost/circular_buffer.hpp>
#include <co/status_codes.hpp>
#include <co/ts/impl/waiting_queue.hpp>
#include <co/until.hpp>

namespace co::ts::impl
{

template <typename T>
struct channel_shared_state
{
public:
    using queue_type = boost::circular_buffer<T>;

    explicit channel_shared_state(size_t capacity)
        : _queue(capacity)
    {}

    std::mutex _mutex;
    bool _closed = false;
    queue_type _queue;
    co::impl::ts_waiting_queue _producer_waiting_queue;
    co::impl::ts_waiting_queue _consumer_waiting_queue;
};

}  // namespace co::ts::impl

namespace co::ts
{

// channel should be passed by value between threads. The channel object is not
// thread safe itself.
template <typename T>
class channel
{
public:
    explicit channel(size_t capacity)
        : _state(std::make_shared<impl::channel_shared_state<T>>(capacity))
    {}

    template <typename T2>
    co::result<void> try_push(T2&& t) requires(std::is_constructible_v<T, T2>);

    template <typename T2>
    co::func<co::result<void>> push(T2&& t) requires(std::is_constructible_v<T, T2>);

    template <typename T2>
    co::result<void> blocking_push(T2&& t) requires(std::is_constructible_v<T, T2>);

    /// non blocking, returns immideately if the channel is blocked
    co::result<T> try_pop();
    /// passes the control to the coroutine scheduler while the channel is blocked
    co::func<co::result<T>> pop();
    /// the OS thread will be suspended while the channel is blocked
    co::result<T> blocking_pop();

    void close();

    [[nodiscard]] bool is_closed() const;

private:
    void check_shared_state() const
    {
        if (_state == nullptr)
            throw std::runtime_error("channel shared state is nullptr");
    }

private:
    std::shared_ptr<impl::channel_shared_state<T>> _state;
};

template <typename T>
template <typename T2>
co::result<void> channel<T>::try_push(T2&& t) requires(std::is_constructible_v<T, T2>)
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

template <typename T>
template <typename T2>
co::func<co::result<void>> channel<T>::push(T2&& t) requires(std::is_constructible_v<T, T2>)
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    if (_state->_closed)
        co_return co::err(co::closed);

    while (_state->_queue.full() && !_state->_closed)
    {
        // `wait` will reacquire the lock
        co_await _state->_producer_waiting_queue.wait(lk);
        // TODO: can return an error?
    }

    if (_state->_closed)
        co_return co::err(co::closed);

    assert(!_state->_queue.full());
    _state->_queue.push_back(std::forward<T2>(t));
    _state->_consumer_waiting_queue.notify_one();
    co_return co::ok();
}

template <typename T>
template <typename T2>
co::result<void> channel<T>::blocking_push(T2&& t) requires(std::is_constructible_v<T, T2>)
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    if (_state->_closed)
        return co::err(co::closed);

    while (_state->_queue.full() && !_state->_closed)
    {
        // `blocking_wait` will reacquire the lock
        _state->_producer_waiting_queue.blocking_wait(lk);
        // TODO: wait can't return an error?
    }

    if (_state->_closed)
        return co::err(co::closed);

    assert(!_state->_queue.full());
    _state->_queue.push_back(std::forward<T2>(t));
    _state->_consumer_waiting_queue.notify_one();
    return co::ok();
}

template <typename T>
co::result<T> channel<T>::try_pop()
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

template <typename T>
co::func<co::result<T>> channel<T>::pop()
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    while (_state->_queue.empty() && !_state->_closed)
    {
        co_await _state->_consumer_waiting_queue.wait(lk);
    }

    if (_state->_closed && _state->_queue.empty())
        co_return co::err(co::closed);

    assert(!_state->_queue.empty());
    result<T> res = co::ok(std::move(_state->_queue.front()));
    _state->_queue.pop_front();
    _state->_producer_waiting_queue.notify_one();
    co_return res;
}

template <typename T>
co::result<T> channel<T>::blocking_pop()
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    while (_state->_queue.empty() && !_state->_closed)
    {
        _state->_consumer_waiting_queue.blocking_wait(lk);
    }

    if (_state->_closed && _state->_queue.empty())
        return co::err(co::closed);

    assert(!_state->_queue.empty());
    result<T> res = co::ok(std::move(_state->_queue.front()));
    _state->_queue.pop_front();
    _state->_producer_waiting_queue.notify_one();
    return res;
}

template <typename T>
void channel<T>::close()
{
    check_shared_state();
    std::unique_lock lk(_state->_mutex);
    _state->_closed = true;
    _state->_producer_waiting_queue.notify_all();
    _state->_consumer_waiting_queue.notify_all();
}

template <typename T>
[[nodiscard]] bool channel<T>::is_closed() const
{
    check_shared_state();
    return _state->_closed;
}

}  // namespace co::ts