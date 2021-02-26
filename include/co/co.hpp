#pragma once

#include <co/channel.hpp>
#include <co/condition_variable.hpp>
#include <co/event.hpp>
#include <co/func.hpp>
#include <co/future.hpp>
#include <co/mutex.hpp>
#include <co/result.hpp>
#include <co/scheduler.hpp>
#include <co/signal.hpp>
#include <co/sleep.hpp>
#include <co/thread.hpp>

namespace co
{

inline void loop()
{
    co::impl::get_scheduler().run();
}

template <FuncLambdaConcept F>
inline void loop(F&& f)
{
    co::thread(std::forward<F>(f), "main").detach();
    co::impl::get_scheduler().run();
}

inline void loop(func<void>&& func)
{
    co::thread(std::move(func), "main").detach();
    loop();
}

}  // namespace co