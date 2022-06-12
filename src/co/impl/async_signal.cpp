#include <co/check.hpp>
#include <co/event.hpp>
#include <co/impl/async_signal.hpp>
#include <co/impl/scheduler.hpp>

namespace co::impl
{

co::func<void> async_signal::close()
{
    // uv_async_t is always active until is deleted.
    CO_DCHECK(uv_is_active((uv_handle_t*)&uv_async) != 0 /*inactive*/);

    event ev;

    uv_async.data = static_cast<void*>(&ev);
    auto on_close = [](uv_handle_t* uv_async)
    {
        CO_DCHECK(uv_async != nullptr);
        CO_DCHECK(uv_async->data != nullptr);

        auto& ev = *static_cast<event*>(uv_async->data);
        ev.notify();
    };

    uv_close((uv_handle_t*)&uv_async, on_close);

    co_await ev.wait();
}

}  // namespace co::impl