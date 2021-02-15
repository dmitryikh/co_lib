#pragma once

#include <functional>
#include <uv.h>

#include <co/exception.hpp>
#include <co/impl/uv_handler.hpp>
#include <co/scheduler.hpp>
#include <co/stop_token.hpp>

namespace co
{

namespace impl
{

using uv_signal_ptr = uv_handle_ptr<uv_signal_t>;

inline uv_signal_ptr make_uv_signal_ptr()
{
    uv_signal_t* signal = new uv_signal_t;
    uv_signal_init(co::impl::get_scheduler().uv_loop(), signal);
    return uv_signal_ptr{ signal };
}

};  // namespace impl

class [[nodiscard]] signal_callback
{
public:
    using callback = std::function<void(int /*signal*/)>;

    signal_callback(int signal, callback callback)
        : _callback(std::move(callback))
    {
        register_signal(signal);
    }

    signal_callback(std::initializer_list<int> signals, callback callback)
        : _callback(std::move(callback))
    {
        for (int signal : signals)
            register_signal(signal);
    }

    ~signal_callback()
    {
        for (auto& signal : _uv_signals)
        {
            assert(signal != nullptr);
            uv_signal_stop(signal.get());
        }
    }

    // no copy, no move because we need to preserve the address of the callback
    // for uv's event loop
    signal_callback(const signal_callback&) = delete;
    signal_callback& operator=(const signal_callback&) = delete;
    signal_callback(signal_callback&&) = delete;
    signal_callback& operator=(signal_callback&&) = delete;

private:
    void register_signal(int signal)
    {
        auto uv_signal_ptr = impl::make_uv_signal_ptr();
        uv_signal_ptr->data = static_cast<void*>(this);
        auto ret = uv_signal_start(uv_signal_ptr.get(), signal_handler, signal);
        if (ret != 0)
            throw co::exception(co::other, uv_strerror(ret));
        _uv_signals.push_back(std::move(uv_signal_ptr));
    }

    static void signal_handler(uv_signal_t* handle, int signal)
    {
        assert(handle != nullptr);
        assert(handle->data != nullptr);

        auto& self = *static_cast<signal_callback*>(handle->data);

        assert(self._callback != nullptr);
        self._callback(signal);
    }

private:
    callback _callback;
    std::vector<impl::uv_signal_ptr> _uv_signals;
};

inline signal_callback signal_ctrl_c(co::stop_source stop_source)
{
    return signal_callback(SIGINT,
                           [stop_source = std::move(stop_source)](auto) mutable { stop_source.request_stop(); });
}

}  // namespace co