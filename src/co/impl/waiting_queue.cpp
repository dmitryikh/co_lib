#include <co/impl/waiting_queue.hpp>

namespace co::impl
{

co::func<void> waiting_queue::wait()
{
    waker w;
    _wakers_list.push_back(w);
    co_await w._event.wait();
    if (w.hook.is_linked())
        w.hook.unlink();
}

co::func<co::result<void>> waiting_queue::wait(const until& until)
{
    waker w;
    _wakers_list.push_back(w);
    auto status = co_await w._event.wait(until);
    if (w.hook.is_linked())
        w.hook.unlink();
    co_return status;
}

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
