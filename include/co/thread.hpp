#pragma once

#include <iostream>
#include <co/func.hpp>
#include <co/impl/shared_state.hpp>
#include <co/impl/thread_storage.hpp>
#include <co/std.hpp>
#include <co/timed_event.hpp>

namespace co
{

namespace impl
{

class thread_func;

class thread_func_promise : public func_promise_base<void>
{
public:
    thread_func get_return_object() noexcept;

    auto final_suspend() noexcept
    {
        return std::suspend_never{};
    }

    void return_void()
    {
        this->_state.set_value();
    }
};

class [[nodiscard]] thread_func
{
public:
    using promise_type = thread_func_promise;

public:
    auto operator co_await() const
    {
        return other_func_awaiter<promise_type>{ _coroutine };
    }

    std::coroutine_handle<promise_type> _coroutine;
};

inline thread_func thread_func_promise::get_return_object() noexcept
{
    using coroutine_handle = std::coroutine_handle<thread_func_promise>;
    return thread_func{ coroutine_handle::from_promise(*this) };
}

inline thread_func create_thread_main_func(func<void> func,
                                           std::shared_ptr<timed_event> finish,
                                           std::shared_ptr<thread_storage> thread_storage)
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

}  // namespace impl

class thread
{
public:
    template <FuncLambdaConcept F>
    explicit thread(F&& f, const std::string& thread_name = "")
        : thread(co::invoke(std::forward<F>(f)), thread_name)
    {}

    explicit thread(func<void>&& func, const std::string& thread_name = "")
        : _thread_storage_ptr(impl::create_thread_storage(thread_name, ++id))
        , _event_ptr(std::make_shared<timed_event>())
        , _thread_func(impl::create_thread_main_func(std::move(func), _event_ptr, _thread_storage_ptr))
    {
        // schedule the thread execution
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

    func<result<void>> join(co::until until)
    {
        co_return co_await _event_ptr->wait(until);
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

inline const std::string& name()
{
    return co::impl::this_thread_storage_ref().name;
}

inline const uint64_t id()
{
    return co::impl::this_thread_storage_ref().id;
}

inline stop_token stop_token()
{
    return co::impl::this_thread_storage_ref().stop.get_token();
}

inline bool stop_requested()
{
    return co::impl::this_thread_storage_ref().stop.stop_requested();
}

};  // namespace this_thread

}  // namespace co