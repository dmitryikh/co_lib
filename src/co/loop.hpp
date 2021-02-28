#pragma once
#include <co/func.hpp>
#include <co/thread.hpp>
#include <co/impl/scheduler.hpp>
#include <co/event.hpp>

namespace co
{

/// \brief runs event loop until all co::threads will be done
inline void loop()
{
    co::impl::get_scheduler().run();
}

/// \brief schedules f as a main co::thread and runs event loop until all co::threads will be done
template <FuncLambdaConcept F>
inline void loop(F&& f)
{
    co::thread(std::forward<F>(f), "main").detach();
    co::impl::get_scheduler().run();
}

/// \brief schedules func as a main co::thread and runs event loop until all co::threads will be done
inline void loop(func<void>&& func)
{
    co::thread(std::move(func), "main").detach();
    loop();
}

}  // namespace co