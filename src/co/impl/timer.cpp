#include <co/impl/timer.hpp>

#include <string>
#include <co/event.hpp>

namespace co::impl
{

co::func<void> timer::close()
{
    CO_DCHECK(_uv_timer_opt.has_value());
    CO_DCHECK(uv_is_active((uv_handle_t*)&(*_uv_timer_opt)) == 0 /*inactive*/);
    CO_DCHECK(_cb == nullptr);
    CO_DCHECK(_data == nullptr);

    event ev;

    _uv_timer_opt->data = static_cast<void*>(&ev);
    auto on_close = [](uv_handle_t* uv_timer)
    {
        CO_DCHECK(uv_timer != nullptr);
        CO_DCHECK(uv_timer->data != nullptr);

        auto& ev = *static_cast<event*>(uv_timer->data);
        ev.notify();
    };

    uv_close((uv_handle_t*)&(*_uv_timer_opt), on_close);

    co_await ev.wait();

    // timer handle is closed now
    _uv_timer_opt = std::nullopt;
}

void timer::init(uv_loop_t* uv_loop)
{
    CO_DCHECK(!_uv_timer_opt.has_value());

    _uv_timer_opt = uv_timer_t{};
    int res = uv_timer_init(uv_loop, &(*_uv_timer_opt));
    using namespace std::string_literals;
    if (res != 0)
        throw std::runtime_error("unable to init a timer:"s + uv_strerror(res));
    _uv_timer_opt->data = static_cast<void*>(this);
}

void timer::set_timer(int64_t milliseconds, timer::callback cb, void* data)
{
    CO_DCHECK(_uv_timer_opt.has_value());
    CO_DCHECK(_cb == nullptr);
    CO_DCHECK(_data == nullptr);

    _cb = cb;
    _data = data;
    int res = uv_timer_start(&(*_uv_timer_opt), on_timer, milliseconds, 0);
    using namespace std::string_literals;
    if (res != 0)
        throw std::runtime_error("can't set a timer:"s + uv_strerror(res));
}

void timer::stop()
{
    CO_DCHECK(_uv_timer_opt.has_value());
    uv_timer_stop(&(*_uv_timer_opt));
    _cb = nullptr;
    _data = nullptr;
}

void timer::on_timer(uv_timer_t* uv_timer)
{
    CO_DCHECK(uv_timer != nullptr);
    CO_DCHECK(uv_timer->data != nullptr);

    auto& self = *static_cast<timer*>(uv_timer->data);
    CO_DCHECK(self._cb != nullptr);
    CO_DCHECK(self._data != nullptr);

    self._cb(self._data);
    self._cb = nullptr;
    self._data = nullptr;
}

}  // namespace co::impl