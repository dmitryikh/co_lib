#pragma once

#include <co/scheduler.hpp>
#include <co/thread.hpp>

namespace co
{

inline void loop()
{
    co::impl::get_scheduler().run();
}

inline void loop(task<void>&& task)
{
    co::thread(std::move(task), "main").detach();
    loop();
}

}