#pragma once

#include <co/func.hpp>
#include <co/impl/waiting_queue.hpp>
#include <co/result.hpp>

namespace co
{

/// \brief is a synchronisation primitive where many sides can wait to be notified and other sides can signal a
/// notification
///
/// Usage:
/// \code
///     co::condition_variable cv;
///     std::string data;
///     bool ready = false;
///
///     auto th = co::thread(
///         [&]() -> co::func<void>
///         {
///           data = "hello world";
///           ready = true;
///           cv.notify_all();
///         },
///         "producer");
///
///     co_await cv.wait([&]() { return ready; }, { 50ms }).unwrap();
///     assert(data == "hello world");
///     co_await th.join();
/// \endcode
class condition_variable
{
public:
    /// \brief unconditionally waits until the condition variable is notified
    ///
    /// notification should be called after wait
    co::func<void> wait();

    /// \brief waits until the condition variable is notified or the operation is interrupted
    ///
    /// notification should be called after wait
    co::func<co::result<void>> wait(const co::until& until);

    /// \brief waits until the condition variable is notified and predicate is satisfied
    ///
    /// returns immediately if predicate is already satisfied
    template <typename Predicate>
    co::func<void> wait(Predicate predicate);

    /// \brief waits until the condition variable is notified and predicate is satisfied or the operation is interrupted
    ///
    /// returns immediately if predicate is already satisfied
    template <typename Predicate>
    co::func<co::result<void>> wait(Predicate predicate, co::until until);

    /// \brief notify one of awaiters to wake up and check their predicates
    void notify_one();

    /// \brief notify all awaiter to wake up and check their predicates
    void notify_all();

private:
    impl::waiting_queue _waiting_queue;
};

template <typename Predicate>
co::func<void> condition_variable::wait(Predicate predicate)
{
    while (!predicate())
    {
        co_await wait();
    }
}

template <typename Predicate>
co::func<co::result<void>> condition_variable::wait(Predicate predicate, co::until until)
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

}  // namespace co