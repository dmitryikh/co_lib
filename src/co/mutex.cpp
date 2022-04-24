#include <co/mutex.hpp>

namespace co
{

co::func<void> mutex::lock()
{
    if (try_lock())
        co_return;

    co_await _waiting_queue.wait();
}

co::func<co::result<void>> mutex::lock(const until& until)
{
    // TODO: check stop token first?
    if (try_lock())
        co_return co::ok();

    co_return co_await _waiting_queue.wait(until);
}

bool mutex::try_lock()
{
    if (_is_locked)
        return false;

    _is_locked = true;
    return true;
}

void mutex::unlock()
{
    // TODO: check that unlock called from the proper coroutine
    if (!_is_locked)
    {
        // TODO: add diagnostic here
        return;
    }

    if (!_waiting_queue.notify_one())
        _is_locked = false;
}

}