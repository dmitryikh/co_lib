#pragma once

#include <queue>
#include <uv.h>
#include <co/std.hpp>
#include <co/task.hpp>

namespace co::impl
{

class scheduler
{
    using coroutine_handle = std::coroutine_handle<>;

public:

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