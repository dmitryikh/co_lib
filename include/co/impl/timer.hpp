#pragma once

#include <optional>
#include <co/func.hpp>
#include <uv.h>

namespace co::impl
{

class timer
{
public:
    using callback = void (*)(void* data);

    timer() = default;

    timer(const timer&) = delete;
    timer(timer&&) = delete;
    timer& operator=(const timer&) = delete;
    timer& operator=(timer&&) = delete;

    void init(uv_loop_t* uv_loop)
    {
        assert(!_uv_timer_opt.has_value());

        _uv_timer_opt = uv_timer_t{};
        int res = uv_timer_init(uv_loop, &(*_uv_timer_opt));
        using namespace std::string_literals;
        if (res != 0)
            throw std::runtime_error("unable to init a timer:"s + uv_strerror(res));
        _uv_timer_opt->data = static_cast<void*>(this);
    }

    void set_timer(int64_t milliseconds, callback cb, void* data)
    {
        assert(_uv_timer_opt.has_value());
        assert(_cb == nullptr);
        assert(_data == nullptr);

        _cb = cb;
        _data = data;
        int res = uv_timer_start(&(*_uv_timer_opt), on_timer, milliseconds, 0);
        using namespace std::string_literals;
        if (res != 0)
            throw std::runtime_error("can't set a timer:"s + uv_strerror(res));
    }

    // NOTE: idempotent
    void stop()
    {
        assert(_uv_timer_opt.has_value());
        uv_timer_stop(&(*_uv_timer_opt));
        _cb = nullptr;
        _data = nullptr;
    }

    co::func<void> close();

private:
    static void on_timer(uv_timer_t* uv_timer)
    {
        assert(uv_timer != nullptr);
        assert(uv_timer->data != nullptr);

        auto& self = *static_cast<timer*>(uv_timer->data);
        assert(self._cb != nullptr);
        assert(self._data != nullptr);

        self._cb(self._data);
        self._cb = nullptr;
        self._data = nullptr;
    }

private:
    std::optional<uv_timer_t> _uv_timer_opt;
    callback _cb = nullptr;
    void* _data = nullptr;
};

}  // namespace co::impl
