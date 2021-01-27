#pragma once

#include <thread>
#include <map>
#include <unordered_map>
#include <queue>
#include <experimental/coroutine>
#include "time.hpp"
#include "task.hpp"

class scheduled_task;

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
        while (!_ready.empty() || !_priority_queue.empty())
        {
            while (!_ready.empty())
            {
                auto coro_handle = _ready.front();
                _ready.pop();
                coro_handle.resume();
            }

            if (!_priority_queue.empty())
            {
                auto [time_point, coro_handle] = *_priority_queue.begin();
                _priority_queue.erase(_priority_queue.begin());
                auto diff = time_point - system_clock::now();
                if (diff > duration{})
                    std::this_thread::sleep_for(diff);
                coro_handle.resume();
            }
        }
    }

    void schedule_at(time_point time, coroutine_handle handle)
    {
        _priority_queue.emplace(time, handle);
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

private:
    std::multimap<time_point, coroutine_handle> _priority_queue;
    std::queue<coroutine_handle> _ready;
    std::unordered_map<void* /*coroutine frame address*/, task<void>> _tasks;
};

inline scheduler& get_scheduler()
{
    static scheduler _scheduler;
    return _scheduler;
}
