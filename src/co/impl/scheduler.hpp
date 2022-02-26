#pragma once

#include <queue>
#include <co/std.hpp>
#include <uv.h>

namespace co::impl
{

/// \brief scheduler is responsible for running event loop, queueing co::threads ready to resume
///
/// user code should not interact with this class
class scheduler
{
    using coroutine_handle = std::coroutine_handle<>;

public:
    /// \brief run event loop until all co::thread will be finished
    ///
    /// run() blocks current OS thread
    void run();

    /// \brief put coroutine handle to the ready queue
    void ready(coroutine_handle handle);

    /// \brief get raw uv_loop object
    uv_loop_t* uv_loop()
    {
        return &_uv_loop;
    }

private:
    /// \brief consumes the ready queue and resume coroutines
    void resume_ready();

private:
    uv_loop_t _uv_loop;
    std::queue<coroutine_handle> _ready;
};

/// \brief returns thread local scheduler object
scheduler& get_scheduler();

}  // namespace co::impl