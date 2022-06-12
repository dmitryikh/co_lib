#include <co/signal_callback.hpp>

#include <co/exception.hpp>

namespace co::impl
{

uv_signal_ptr make_uv_signal_ptr()
{
    auto signal = new uv_signal_t;
    uv_signal_init(co::impl::get_scheduler().uv_loop(), signal);
    return uv_signal_ptr{ signal };
}

}  // namespace co::impl

namespace co
{

signal_callback::signal_callback(int signal, co::signal_callback::callback callback)
    : _callback(std::move(callback))
{
    register_signal(signal);
}

signal_callback::signal_callback(std::initializer_list<int> signals, signal_callback::callback callback)
    : _callback(std::move(callback))
{
    for (int signal : signals)
        register_signal(signal);
}

signal_callback::~signal_callback()
{
    for (auto& signal : _uv_signals)
    {
        CO_DCHECK(signal != nullptr);
        uv_signal_stop(signal.get());
    }
}

void signal_callback::register_signal(int signal)
{
    auto uv_signal_ptr = impl::make_uv_signal_ptr();
    uv_signal_ptr->data = static_cast<void*>(this);
    auto ret = uv_signal_start(uv_signal_ptr.get(), signal_handler, signal);
    if (ret != 0)
        throw co::exception(co::other, uv_strerror(ret));
    _uv_signals.push_back(std::move(uv_signal_ptr));
}

void signal_callback::signal_handler(uv_signal_t* handle, int signal)
{
    CO_DCHECK(handle != nullptr);
    CO_DCHECK(handle->data != nullptr);

    auto& self = *static_cast<signal_callback*>(handle->data);

    CO_DCHECK(self._callback != nullptr);
    self._callback(signal);
}

co::signal_callback signal_ctrl_c(co::stop_source stop_source)
{
    return signal_callback(SIGINT,
                           [stop_source = std::move(stop_source)](auto) mutable { stop_source.request_stop(); });
}

}  // namespace co
