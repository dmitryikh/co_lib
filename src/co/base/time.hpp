#pragma once

#include <chrono>

namespace co::base
{

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

}