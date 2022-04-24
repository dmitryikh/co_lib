#include <co/impl/scheduler.hpp>
#include <co/impl/thread_storage.hpp>

namespace co::impl
{
void wake_thread(thread_storage* thread)
{
    assert(thread != nullptr);
    if (&get_scheduler() == thread->scheduler_ptr)
    {
        // We are currently in the same thread where the co::thread lives.
        // So we can schedule the corourine without std::thread sync.
        assert(thread->suspended_coroutine.address() != nullptr);
        thread->scheduler_ptr->ready(thread->suspended_coroutine);
        thread->suspended_coroutine = std::coroutine_handle<>{};
    }
    else
    {
        thread->async_signal.send();
    }
}
}  // namespace co::impl