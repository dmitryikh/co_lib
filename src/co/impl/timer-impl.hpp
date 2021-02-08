#pragma once

#include <co/impl/timer.hpp>
#include <co/event.hpp>

namespace co::impl
{

co::func<void> timer::close()
{
    assert(_uv_timer_opt.has_value());
    assert(uv_is_active((uv_handle_t*) &(*_uv_timer_opt)) == 0 /*inactive*/);
    assert(_cb == nullptr);
    assert(_data == nullptr);

    event ev;

    _uv_timer_opt->data = static_cast<void*>(&ev);
    auto on_close = [] (uv_handle_t* uv_timer)
    {
        assert(uv_timer != nullptr);
        assert(uv_timer->data != nullptr);

        auto& ev = *static_cast<event*>(uv_timer->data);
        ev.notify();
    };

    uv_close((uv_handle_t*) &(*_uv_timer_opt), on_close);

    co_await ev.wait();

    // timer handle is closed now
    _uv_timer_opt = std::nullopt;
}

}  // namespace co::impl