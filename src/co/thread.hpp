#pragma once

#include <co/std.hpp>
#include <co/impl/shared_state.hpp>
#include <co/impl/awaitable_base.hpp>
#include <co/impl/thread_storage.hpp>
#include <co/event.hpp>

namespace co
{

namespace impl
{

inline task<void, true> create_thread_main_task(task<void> task, std::shared_ptr<event> finish, std::shared_ptr<thread_storage> thread_storage)
{
    try
    {
        set_this_thread_storage_ptr(thread_storage.get());
        co_await task;
    }
    catch (const std::exception& exc)
    {
        // TODO: terminate here
        std::cerr << "task error: " << exc.what() << "\n";
    }
    finish->notify();
}

} // namespace impl


class thread
{
public:

    explicit thread(task<void>&& task, const std::string& thread_name = "")
        : _thread_storage_ptr(impl::create_thread_storage(thread_name, ++id))
        , _event_ptr(std::make_shared<event>())
        , _thread_task(impl::create_thread_main_task(std::move(task), _event_ptr, _thread_storage_ptr))
    {
        co::impl::get_scheduler().ready(_thread_task._coroutine);
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

    task<void> join()
    {
        co_await _event_ptr->wait();
    }

    task<status> join(const stop_token& token)
    {
        co_return co_await _event_ptr->wait(token);
    }

    template <class Clock, class Duration>
    task<status> join_until(std::chrono::time_point<Clock, Duration> sleep_time, const co::stop_token& token = {})
    {
        co_await _event_ptr->wait_until(sleep_time, token);
    }

    template <class Rep, class Period>
    task<status> join_for(std::chrono::duration<Rep, Period> sleep_duration, const co::stop_token& token = {})
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
    std::shared_ptr<event> _event_ptr;
    task<void, true> _thread_task;
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