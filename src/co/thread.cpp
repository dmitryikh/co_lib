#include <co/thread.hpp>

#include <iostream>
#include <co/check.hpp>
#include <co/event.hpp>
#include <co/impl/scheduler.hpp>
#include <co/impl/thread_storage.hpp>

namespace co::impl
{

thread_func thread_func_promise::get_return_object() noexcept
{
    using coroutine_handle = std::coroutine_handle<thread_func_promise>;
    return thread_func{ coroutine_handle::from_promise(*this) };
}

thread_func create_thread_main_func(func<void> func,
                                    std::shared_ptr<event> finish,
                                    std::shared_ptr<thread_storage> thread_storage)
{
    try
    {
        set_this_thread_storage_ptr(thread_storage.get());
        thread_storage->_timer.init(co::impl::get_scheduler().uv_loop());
        thread_storage->async_signal.init(co::impl::get_scheduler().uv_loop());
        co_await func;
    }
    catch (const std::exception& exc)
    {
        CO_CHECK(true) << "unhandled exception: " << exc.what();
    }
    co_await thread_storage->_timer.close();
    co_await thread_storage->async_signal.close();
    finish->notify();
    set_this_thread_storage_ptr(nullptr);
}

}  // namespace co::impl

namespace co
{

thread::thread(co::func<void>&& func, const std::string& thread_name)
    : _thread_storage_ptr(impl::create_thread_storage(thread_name, ++id, &co::impl::get_scheduler()))
    , _event_ptr(std::make_shared<event>())
    , _thread_func(impl::create_thread_main_func(std::move(func), _event_ptr, _thread_storage_ptr))
{
    // schedule the thread execution
    co::impl::get_scheduler().ready(_thread_func._coroutine);
}

thread::~thread()
{
    // If _event_ptr == null, then the thread object has been already moved away.
    if (_event_ptr != nullptr) {
        CO_CHECK(_detached || is_joined())
            << "Undetached thread should be joined before being destructed. Thread name = "
            << _thread_storage_ptr->name << " id = " << _thread_storage_ptr->id;
    }
}

co::func<void> thread::join()
{
    co_await _event_ptr->wait();
}

co::func<co::result<void>> thread::join(co::until until)
{
    // TODO: event is one off mechanism, probably we want to call thread::join several times.
    co_return co_await _event_ptr->wait(until);
}

bool thread::is_joined() const
{
    return _event_ptr->is_notified();
}

co::stop_source thread::get_stop_source() const
{
    return _thread_storage_ptr->stop;
}

co::stop_token thread::get_stop_token() const
{
    return _thread_storage_ptr->stop.get_token();
}

void thread::request_stop() const
{
    _thread_storage_ptr->stop.request_stop();
}

}  // namespace co