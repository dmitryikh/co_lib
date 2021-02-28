#pragma once

#include <functional>
#include <uv.h>

#include <co/impl/scheduler.hpp>
#include <co/impl/uv_handler.hpp>
#include <co/stop_token.hpp>

namespace co::impl
{

using uv_signal_ptr = uv_handle_ptr<uv_signal_t>;

}  // namespace co::impl

namespace co
{

/// \brief registers callbacks for OS signals. Signals will be unregistered when signal_callback is destroyed
///
/// Usage:
/// \code
///     auto scoped_signal = co::signal_callback(
///         SIGINT,
///         [stop_source] (int signal) mutable { stop_source.request_stop(); });
/// \endcode
class [[nodiscard]] signal_callback
{
public:
    /// \brief signature of the callback function
    using callback = std::function<void(int /*signal*/)>;

    /// \brief registers the callback to be called when the signal occurs
    signal_callback(int signal, callback callback);

    /// \brief registers the callback to be called when any of the signals occurs
    signal_callback(std::initializer_list<int> signals, callback callback);

    ~signal_callback();

    // no copy, no move because we need to preserve the address of the callback
    // for uv's event loop
    signal_callback(const signal_callback&) = delete;
    signal_callback& operator=(const signal_callback&) = delete;
    signal_callback(signal_callback&&) = delete;
    signal_callback& operator=(signal_callback&&) = delete;

private:
    void register_signal(int signal);
    static void signal_handler(uv_signal_t* handle, int signal);

private:
    callback _callback;
    std::vector<impl::uv_signal_ptr> _uv_signals;
};

/// \brief registers the stop_source with CTRL+C signal
///
/// Usage:
/// \code
///     co::stop_source stop_source;
///     auto signal_scoped = co::signal_ctrl_c(stop_source);
///     co_await long_foo(co::until::cancel(stop_source.get_token()));
/// \endcode
co::signal_callback signal_ctrl_c(co::stop_source stop_source);

}  // namespace co