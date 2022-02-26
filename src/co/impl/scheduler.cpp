#include <cassert>
#include <co/impl/scheduler.hpp>

namespace co::impl
{

void scheduler::run()
{
    uv_loop_init(&_uv_loop);
    uv_prepare_t uv_prepare;
    uv_prepare_init(&_uv_loop, &uv_prepare);
    uv_prepare.data = static_cast<void*>(this);

    auto cb = [](uv_prepare_t* h)
    {
        auto& self = *static_cast<scheduler*>(h->data);
        self.resume_ready();

        uv_unref((uv_handle_t*)h);
        if (uv_loop_alive(&self._uv_loop) == 0)
        {
            uv_close((uv_handle_t*)h, /*on_close*/nullptr);
        }
        else
        {
            uv_ref((uv_handle_t*)h);
        }
    };
    uv_prepare_start(&uv_prepare, cb);
    uv_run(&_uv_loop, UV_RUN_DEFAULT);
    uv_loop_close(&_uv_loop);
}

void scheduler::ready(scheduler::coroutine_handle handle)
{
    assert(handle);
    _ready.push(handle);
}

void scheduler::resume_ready()
{
    while (!_ready.empty())
    {
        auto coro_handle = _ready.front();
        _ready.pop();
        coro_handle.resume();
    }
}

scheduler& get_scheduler()
{
    static scheduler _scheduler;
    return _scheduler;
}

}  // namespace co::impl