#pragma once

#include <co/impl/waiting_queue.hpp>

namespace co
{

class condition_variable
{
public:
    /// set of wait methods without a mutex
    func<void> wait()
    {
        co_await _waiting_queue.wait();
    }

    func<result<void>> wait(co::until until)
    {
        return _waiting_queue.wait(until);
    }

    template <typename Predicate>
    func<void> wait(Predicate predicate)
    {
        while (!predicate())
        {
            co_await wait();
        }
    }

    template <typename Predicate>
    func<result<void>> wait(Predicate predicate, co::until until)
    {
        while (!predicate())
        {
            auto res = co_await _waiting_queue.wait(until);
            if (res == co::timeout && predicate())
                co_return co::ok();
            else if (res.is_err())
                co_return res;
        }
        co_return co::ok();
    }

    void notify_one()
    {
        _waiting_queue.notify_one();
    }

    void notify_all()
    {
        _waiting_queue.notify_all();
    }

private:
    co::impl::waiting_queue _waiting_queue;
};

}  // namespace co