#pragma once

#include <queue>
#include <experimental/coroutine>
#include <uv.h>
#include <co/base/time.hpp>
#include <co/base/task.hpp>

namespace co::base
{

class scheduled_task;
class scheduler;
scheduler& get_scheduler();

class scheduled_task_promise
{
public:
    scheduled_task_promise() = default;

    scheduled_task get_return_object();

    constexpr std::experimental::suspend_always initial_suspend() const noexcept { return {}; }
    constexpr std::experimental::suspend_never final_suspend() const noexcept { return {}; }

    void unhandled_exception() {}
    void return_void() {}
};

class scheduled_task
{
public:
    using promise_type = scheduled_task_promise;
    std::experimental::coroutine_handle<> coroutine_handle;
};

scheduled_task scheduled_task_promise::get_return_object()
{
    using coroutine_handle = std::experimental::coroutine_handle<scheduled_task_promise>;
    return { coroutine_handle::from_promise(*this) };
}

class scheduler
{
    using coroutine_handle = std::experimental::coroutine_handle<>;

public:

    void spawn(task<void> task)
    {
        auto sched_task = create_scheduled_task(std::move(task));
        _ready.push(sched_task.coroutine_handle);
    }

    void run()
    {
        while (true)
        {
            resume_ready();
            const int ret = uv_run(uv_default_loop(), UV_RUN_ONCE);
            if (ret == 0 && _ready.empty())
                break;
        }
        uv_loop_close(uv_default_loop());
    }

    void ready(coroutine_handle handle)
    {
        _ready.push(handle);
    }

    uv_loop_t* uv_loop()
    {
        return uv_default_loop();
    }

private:
    static scheduled_task create_scheduled_task(task<void> task)
    {
        try
        {
            co_await task;
        }
        catch (const std::exception& exc)
        {
            std::cerr << "task error: " << exc.what() << "\n";
        }
    }

    void resume_ready()
    {
        while (!_ready.empty())
        {
            auto coro_handle = _ready.front();
            _ready.pop();
            coro_handle.resume();
        }
    }

private:
    std::queue<coroutine_handle> _ready;
};

inline scheduler& get_scheduler()
{
    static scheduler _scheduler;
    return _scheduler;
}

}