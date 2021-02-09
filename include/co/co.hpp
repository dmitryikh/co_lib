#pragma once

#include <co/scheduler.hpp>
#include <co/thread.hpp>
#include <co/result.hpp>
#include <co/func.hpp>
#include <co/mutex.hpp>
#include <co/sleep.hpp>
#include <co/channel.hpp>
#include <co/condition_variable.hpp>
#include <co/one_shot.hpp>
#include <co/future.hpp>

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