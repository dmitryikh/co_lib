#pragma once

#include <boost/circular_buffer.hpp>
#include <co/event.hpp>
#include <co/impl/waiting_queue.hpp>
#include <co/status_code.hpp>

namespace co::impl
{

enum class channel_code
{
    empty = 1,
    full = 2,
    closed = 3
};

class channel_code_category : public status_category
{
    constexpr static uint64_t id = 0x853350d6a1dd9bee;

public:
    channel_code_category()
        : co::status_category(id)
    {}

    [[nodiscard]] const char* name() const noexcept override
    {
        return "co_lib";
    }

    [[nodiscard]] const char* message(int ev) const noexcept override
    {
        switch (static_cast<channel_code>(ev))
        {
        case channel_code::empty:
            return "empty";
        case channel_code::full:
            return "full";
        case channel_code::closed:
            return "closed";
        }
        assert(false);
        return "undefined";
    }
};

inline co::status_code make_status_code(channel_code e)
{
    const static channel_code_category global_channel_code_category;
    return co::status_code{ e, &global_channel_code_category };
}

template <typename T>
struct channel_shared_state
{
public:
    using queue_type = boost::circular_buffer<T>;

    explicit channel_shared_state(size_t capacity)
        : _queue(capacity)
    {}

    bool _closed = false;
    queue_type _queue;
    impl::waiting_queue _producer_waiting_queue;
    impl::waiting_queue _consumer_waiting_queue;
};

}  // namespace co::impl

namespace co
{

const auto full = impl::channel_code::full;
const auto empty = impl::channel_code::empty;
const auto closed = impl::channel_code::closed;

template <typename T>
class channel
{

public:
    explicit channel(size_t capacity)
        : _state(std::make_shared<impl::channel_shared_state<T>>(capacity))
    {}

    template <typename T2>
    result<void> try_push(T2&& t)
    {
        check_shared_state();
        if (_state->_closed)
            return co::err(co::closed);

        if (_state->_queue.full())
            return co::err(co::full);

        _state->_queue.push_back(std::forward<T2>(t));
        _state->_consumer_waiting_queue.notify_one();
        return co::ok();
    }

    template <typename T2>
    func<result<void>> push(T2&& t, co::until until = {})
    {
        check_shared_state();
        if (_state->_closed)
            co_return co::err(co::closed);

        while (_state->_queue.full() && !_state->_closed)
        {
            auto res = co_await _state->_producer_waiting_queue.wait(until);
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

    result<T> try_pop()
    {
        check_shared_state();
        if (_state->_closed && _state->_queue.empty())
            return co::err(co::closed);

        if (_state->_queue.empty())
            return co::err(co::empty);

        result<T> res = co::ok(std::move(_state->_queue.front()));
        _state->_queue.pop_front();
        _state->_producer_waiting_queue.notify_one();
        return res;
    }

    func<result<T>> pop(co::until until = {})
    {
        check_shared_state();
        while (_state->_queue.empty() && !_state->_closed)
        {
            auto res = co_await _state->_consumer_waiting_queue.wait(until);
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

    void close()
    {
        check_shared_state();
        _state->_closed = true;
        _state->_producer_waiting_queue.notify_all();
        _state->_consumer_waiting_queue.notify_all();
    }

    [[nodiscard]] bool is_closed() const
    {
        check_shared_state();
        return _state->_closed;
    }

private:
    void check_shared_state() const
    {
        if (_state == nullptr)
            throw std::runtime_error("channel shared state is nullptr");
    }

private:
    std::shared_ptr<impl::channel_shared_state<T>> _state;
};

}  // namespace co