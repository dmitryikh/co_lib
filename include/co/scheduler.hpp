#pragma once

#include <cassert>
#include <queue>
#include <uv.h>
#include <co/std.hpp>

namespace co::impl
{

class scheduler
{
    using coroutine_handle = std::coroutine_handle<>;

public:

    void run()
    {
        uv_loop_init(&_uv_loop);
        uv_prepare_t uv_prepare;
        uv_prepare_init(&_uv_loop, &uv_prepare);
        uv_prepare.data = static_cast<void*>(this);

        auto cb = [] (uv_prepare_t* h)
        {
            auto& self = *static_cast<scheduler*>(h->data);
            self.resume_ready();

            uv_unref((uv_handle_t*)h);
            if (uv_loop_alive(&self._uv_loop) == 0)
            {
                uv_stop(&self._uv_loop);
            }
            uv_ref((uv_handle_t*)h);
        };
        uv_prepare_start(&uv_prepare, cb);
        uv_run(&_uv_loop, UV_RUN_DEFAULT);
        uv_loop_close(&_uv_loop);
    }

    void ready(coroutine_handle handle)
    {
        assert(handle);
        _ready.push(handle);
    }

    uv_loop_t* uv_loop()
    {
        return &_uv_loop;
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
    uv_loop_t _uv_loop;
    std::queue<coroutine_handle> _ready;
};

inline scheduler& get_scheduler()
{
    static scheduler _scheduler;
    return _scheduler;
}

}