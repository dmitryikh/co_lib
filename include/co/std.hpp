#pragma once

#if __has_include(<coroutine>)
#include <coroutine>
#else
#include <experimental/coroutine>
namespace std
{

using std::experimental::coroutine_handle;
using std::experimental::noop_coroutine;
using std::experimental::suspend_always;
using std::experimental::suspend_never;

}  // namespace std
#endif