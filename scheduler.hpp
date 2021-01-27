#pragma once

#include <thread>
#include <map>
#include <unordered_map>
#include <queue>
#include <experimental/coroutine>
#include <uv.h>
#include "time.hpp"
#include "task.hpp"


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
        uv_idle_t idler;
        uv_idle_init(uv_default_loop(), &idler);
        uv_idle_start(&idler, on_idle);

        resume_ready();
        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        uv_loop_close(uv_default_loop());
    }

    void ready(coroutine_handle handle)
    {
        _ready.push(handle);
    }

    void schedule_at(uv_timer_t& timer_req, uint64_t milliseconds, coroutine_handle handle)
    {
        uv_timer_init(uv_default_loop(), &timer_req);
        timer_req.data = handle.address();
        uv_timer_start(&timer_req, on_timer, milliseconds, 0);
    }

private:
    static scheduled_task create_scheduled_task(task<void> task)
    {
        try
        {
            get_scheduler()._task_counter++;
            co_await task;
        }
        catch (const std::exception& exc)
        {
            std::cerr << "task error: " << exc.what() << "\n";
        }
        get_scheduler()._task_counter--;
    }

    static void on_idle(uv_idle_t* handle)
    {
        assert(handle != nullptr);

        auto& scheduler = get_scheduler();
        if (scheduler.task_running() == 0)
        {
            assert(scheduler._ready.empty());
            // all tasks been finished. Stop the loop by stopping the idle handle
            uv_idle_stop(handle);
        }
        get_scheduler().resume_ready();
    }

    static void on_timer(uv_timer_t* timer_req)
    {
        assert(timer_req != nullptr);
        auto coro = coroutine_handle::from_address(timer_req->data);
        get_scheduler().ready(coro);
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

    bool task_running() const
    {
        return _task_counter;
    }

private:
    std::queue<coroutine_handle> _ready;
    int64_t _task_counter;
};

inline scheduler& get_scheduler()
{
    static scheduler _scheduler;
    return _scheduler;
}
