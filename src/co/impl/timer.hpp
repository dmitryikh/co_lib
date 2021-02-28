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

    void init(uv_loop_t* uv_loop);

    void set_timer(int64_t milliseconds, callback cb, void* data);

    // NOTE: idempotent
    void stop();

    co::func<void> close();

private:
    static void on_timer(uv_timer_t* uv_timer);

private:
    std::optional<uv_timer_t> _uv_timer_opt;
    callback _cb = nullptr;
    void* _data = nullptr;
};

}  // namespace co::impl
