#include <co/ts/impl/waiting_queue.hpp>

namespace co::ts::impl
{

co::func<void> waiting_queue::wait(std::unique_lock<std::mutex>& lk)
{
    assert(lk.owns_lock());
    waker w;
    _wakers_list.push_back(w);
    lk.unlock();
    // NOTE: the event might have been notified after this line
    // TODO: add random sleep here for data race tests
    co_await w._event.wait();
    lk.lock();
    if (w.hook.is_linked())
        w.hook.unlink();
}

void waiting_queue::blocking_wait(std::unique_lock<std::mutex>& lk)
{
    waker w;
    _wakers_list.push_back(w);
    lk.unlock();
    w._event.blocking_wait();
    lk.lock();
    if (w.hook.is_linked())
        w.hook.unlink();
}

// TODO: If we notify under a lock, can we use simpler event class without atomics?
// That may not be working with timeout and stop_tokens (they will be triggered without a mutex)
bool waiting_queue::notify_one()
{
    while (!_wakers_list.empty())
    {
        waker& w = _wakers_list.front();
        _wakers_list.pop_front();
        if (w.wake())
            return true;
    }
    return false;
}

void waiting_queue::notify_all()
{
    while (!_wakers_list.empty())
    {
        waker& w = _wakers_list.front();
        _wakers_list.pop_front();
        w.wake();
    }
}

}
