#pragma once

#include <functional>
#include <co/impl/intrusive_list.hpp>

namespace co
{

using stop_callback_func = std::function<void()>;

namespace impl
{

struct stop_callback_node
{
    stop_callback_func callback;
    intrusive_list_hook hook;

    explicit stop_callback_node(stop_callback_func&& callback)
        : callback(callback)
    {}
};

using stop_callback_list = intrusive_list<stop_callback_node, &stop_callback_node::hook>;

class stop_state
{
public:
    void register_callback(stop_callback_node& callback)
    {
        assert(!callback.hook.is_linked());
        _callbacks.push_back(callback);
    }

    static void unregister_callback(stop_callback_node& callback)
    {
        assert(callback.hook.is_linked());
        callback.hook.unlink();
    }

    void request_stop()
    {
        if (_stop_requested)
            return;
        _stop_requested = true;

        for (const auto& node : _callbacks)
        {
            // TODO: do we need to catch exceptions here?
            node.callback();
        }
    }

    [[nodiscard]] bool stop_requested() const
    {
        return _stop_requested;
    }

    ~stop_state()
    {
        assert(_callbacks.empty());
    }

private:
    bool _stop_requested = false;
    stop_callback_list _callbacks;
};

using stop_state_sptr = std::shared_ptr<stop_state>;

}  // namespace impl

class stop_token
{
    friend class stop_callback;
    friend class stop_source;

    stop_token() = default;

    explicit stop_token(impl::stop_state_sptr stop_state)
        : _stop_state(std::move(stop_state))
    {}

public:
    [[nodiscard]] bool stop_requested() const
    {
        if (!_stop_state)
            return false;

        return _stop_state->stop_requested();
    }

private:
    [[nodiscard]] impl::stop_state_sptr stop_state() const
    {
        return _stop_state;
    }

private:
    impl::stop_state_sptr _stop_state;
};

class stop_source
{
public:
    stop_source()
        : _stop_state(std::make_shared<impl::stop_state>())
    {}

    [[nodiscard]] stop_token get_token() const
    {
        return stop_token(_stop_state);
    }

    void request_stop()
    {
        if (!_stop_state)
            return;

        _stop_state->request_stop();
    }

    [[nodiscard]] bool stop_requested() const
    {
        if (!_stop_state)
            return false;

        return _stop_state->stop_requested();
    }

private:
    impl::stop_state_sptr _stop_state;
};

class stop_callback
{
public:
    stop_callback(const stop_token& token, stop_callback_func&& callback)
        : _stop_state(token.stop_state())
        , _callback_node(std::move(callback))
    {
        if (!_stop_state)
            return;

        if (_stop_state->stop_requested())
            _callback_node.callback();
        else
            _stop_state->register_callback(_callback_node);
    }

    ~stop_callback()
    {
        if (!_stop_state)
            return;

        _stop_state->unregister_callback(_callback_node);
    }

    // no copy, no move because we need to preserver the address of the function
    stop_callback(const stop_callback&) = delete;
    stop_callback& operator=(const stop_callback&) = delete;
    stop_callback(stop_callback&&) = delete;
    stop_callback& operator=(stop_callback&&) = delete;

private:
    impl::stop_state_sptr _stop_state;
    impl::stop_callback_node _callback_node;
};

}  // namespace co