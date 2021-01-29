#pragma once

#include <iomanip> // TODO
#include <co/std.hpp>
#include <co/impl/shared_state.hpp>
#include <co/impl/awaitable_base.hpp>
#include <co/impl/thread_storage.hpp>

namespace co
{

class thread;

namespace impl
{

class thread_main_task;

class thread_main_task_promise
{
    friend class thread_main_task;
public:
    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        return symmetric_transfer_awaitable{ _continuation, _destroy };
    }

    void set_continuation(std::coroutine_handle<> continuation) noexcept
    {
        _continuation = continuation;
    }

    thread_main_task get_return_object() noexcept;

    void unhandled_exception()
    {
        // NOTE: got exception to the co::thread root
        std::terminate();
    }

    void return_void()
    {
        _state.set_value();
    }

    shared_state<void>& state()
    {
        return _state;
    }

private:
    bool _destroy = false;
    std::coroutine_handle<> _continuation;
    shared_state<void> _state;
};

class thread_main_task
{
public:
    friend class co::thread;
    using promise_type = thread_main_task_promise;

private:

    class awaitable : public co::impl::awaitable_base
    {
        using base = co::impl::awaitable_base;
    public:
        awaitable(std::coroutine_handle<promise_type> coroutine) noexcept
            : _coroutine(coroutine)
        {}

        bool await_ready() const noexcept
        {
            return !_coroutine || _coroutine.done();
        }

        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> continuation) noexcept
        {
            _coroutine.promise().set_continuation(continuation);
            // base::await_suspend(continuation);
            // _coroutine is already run. Just return the control to the scheduler
            return std::noop_coroutine();
        }

        void await_resume()
        {
            // base::await_resume();
            return _coroutine.promise().state().value();
        }

    public:
        std::coroutine_handle<promise_type> _coroutine;
    };

public:

    explicit thread_main_task(std::coroutine_handle<promise_type> coroutine, shared_state<void>& state)
        : _coroutine(coroutine)
        , _state(state)
    {}

    thread_main_task(thread_main_task&& t) noexcept
        : thread_main_task(t._coroutine, t._state)
    {
        t._coroutine = nullptr;
    }

    thread_main_task(const thread_main_task&) = delete;
    thread_main_task& operator=(const thread_main_task&) = delete;
    thread_main_task& operator=(thread_main_task&& other) = delete;

    ~thread_main_task()
    {
        if (_coroutine && !_coroutine.promise()._destroy)
        {
            _coroutine.destroy();
        }
    }

    bool is_done() const
    {
        return _state.is_done();
    }

    auto operator co_await() const
    {
        return awaitable{ _coroutine };
    }

    void detach()
    {
        _coroutine.promise()._destroy = true;
    }

private:
    std::coroutine_handle<promise_type> _coroutine;
    shared_state<void>& _state;
};

thread_main_task thread_main_task_promise::get_return_object() noexcept
{
    using coroutine_handle = std::coroutine_handle<thread_main_task_promise>;
    return thread_main_task{ coroutine_handle::from_promise(*this), _state };
}

inline thread_main_task create_thread_main_task(task<void> task, std::shared_ptr<thread_storage> thread_storage)
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
}

} // namespace impl


class thread
{
public:

    explicit thread(task<void>&& task, const std::string& thread_name = "")
        : _thread_storage_ptr(impl::create_thread_storage(thread_name, ++id))
        , _thread_task(impl::create_thread_main_task(std::move(task), _thread_storage_ptr))
    {
        co::impl::get_scheduler().ready(_thread_task._coroutine);
    }

    ~thread()
    {
        if (_detached)
            return;

        if (!is_joined())
        {
            detach();
            // TODO: need to throw or terminate
            // throw std::runtime_error("thread should be joined before destruction");
        }
    }
    
    // co_await th.join();
    impl::thread_main_task& join()
    {
        if (_detached)
            throw std::runtime_error("thread is detached");
        return _thread_task;
    }

    void detach()
    {
        if (_detached)
            return;
        _detached = true;
        _thread_task.detach();
    }

    bool is_joined() const
    {
        if (_detached)
            throw std::runtime_error("thread is detached");

        return _thread_task.is_done();
    }

private:
    static inline uint64_t id = 0;

    bool _detached = false;

    std::shared_ptr<impl::thread_storage> _thread_storage_ptr;
    impl::thread_main_task _thread_task;
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
};


}