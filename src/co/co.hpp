#pragma once

#include <co/scheduler.hpp>
#include <co/thread.hpp>

namespace co
{

inline void loop()
{
    co::impl::get_scheduler().run();
}

inline void loop(func<void>&& func)
{
    co::thread(std::move(func), "main").detach();
    loop();
}

}