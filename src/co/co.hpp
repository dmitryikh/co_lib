#pragma once

#include <co/scheduler.hpp>
#include <co/thread.hpp>

namespace co
{

inline void loop(task<void>&& task)
{
    co::thread(std::move(task), "main").detach();
    co::impl::get_scheduler().run();
}

}