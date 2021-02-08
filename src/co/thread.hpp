#pragma once

#include <iostream>
#include <co/std.hpp>
#include <co/impl/shared_state.hpp>
#include <co/impl/thread_storage.hpp>
#include <co/timed_event.hpp>
#include <co/func.hpp>

namespace co
{

namespace impl
{

inline thread_func create_thread_main_func(func<void> func, std::shared_ptr<timed_event> finish, std::shared_ptr<thread_storage> thread_storage)
{
    try
    {
        set_this_thread_storage_ptr(thread_storage.get());
        thread_storage->_timer.init(co::impl::get_scheduler().uv_loop());
        co_await func;
    }
    catch (const std::exception& exc)
    {
        // TODO: terminate here
        std::cerr << "func error: " << exc.what() << "\n";
    }
    co_await thread_storage->_timer.close();
    finish->notify();
    set_this_thread_storage_ptr(nullptr);
}

} // namespace impl


class thread
{
public:

    explicit thread(func<void>&& func, const std::string& thread_name = "")
        : _thread_storage_ptr(impl::create_thread_storage(thread_name, ++id))
        , _event_ptr(std::make_shared<timed_event>())
        , _thread_func(impl::create_thread_main_func(std::move(func), _event_ptr, _thread_storage_ptr))
    {
        co::impl::get_scheduler().ready(_thread_func._coroutine);
    }

    ~thread()
    {
        if (!_detached && !is_joined())
        {
            // TODO: terminate is to offensive, diagnostic needed
            // std::terminate();
        }
    }

    void detach()
    {
        _detached = true;
    }

    func<void> join()
    {
        co_await _event_ptr->wait();
    }

    func<result<void>> join(const stop_token& token)
    {
        co_return co_await _event_ptr->wait(token);
    }

    template <class Clock, class Duration>
    func<result<void>> join_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = {})
    {
        co_await _event_ptr->wait_until(sleep_time, token);
    }

    template <class Rep, class Period>
    func<result<void>> join_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token = {})
    {
        co_await _event_ptr->wait_for(sleep_duration, token);
    }

    bool is_joined() const
    {
        return _event_ptr->is_notified();
    }

    stop_source get_stop_source() const
    {
        return _thread_storage_ptr->stop;
    }

    stop_token get_stop_token() const
    {
        return _thread_storage_ptr->stop.get_token();
    }

    void request_stop() const
    {
        _thread_storage_ptr->stop.request_stop();
    }

private:
    static inline uint64_t id = 0;

    bool _detached = false;
    std::shared_ptr<impl::thread_storage> _thread_storage_ptr;
    std::shared_ptr<timed_event> _event_ptr;
    impl::thread_func _thread_func;
};

namespace this_thread
{
    const std::string& get_name()
    {
        return co::impl::this_thread_storage_ref().name;
    }

    const uint64_t get_id()
    {
        return co::impl::this_thread_storage_ref().id;
    }

    stop_token get_stop_token()
    {
        return co::impl::this_thread_storage_ref().stop.get_token();
    }

    bool stop_requested()
    {
        return co::impl::this_thread_storage_ref().stop.stop_requested();
    }
};


}