#include <co/condition_variable.hpp>

namespace co
{

co::func<void> condition_variable::wait()
{
    co_await _waiting_queue.wait();
}

co::func<co::result<void>> condition_variable::wait(co::until until)
{
    return _waiting_queue.wait(until);
}

void condition_variable::notify_one()
{
    _waiting_queue.notify_one();
}

void condition_variable::notify_all()
{
    _waiting_queue.notify_all();
}

}  // namespace co