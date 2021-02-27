#pragma once

#include <list>
#include <co/impl/scheduler.hpp>
#include <co/impl/waiting_queue.hpp>
#include <co/std.hpp>
#include <co/stop_token.hpp>
#include <co/until.hpp>

namespace co
{

/// \brief non blocking mutex
///
/// Trying to get the lock twice without unlock() leads to deadlock
///
/// Usage:
/// \code
///     co::mutex mtx;
///     auto th = co::thread([&mtx]() {
///         co_await mtx.lock();
///         do_job_exclusively();
///         mtx.unlock();
///     });
///     co_await mtx.lock();
///     do_job_exclusively();
///     mtx.unlock();
///     co_await th.join();
/// \endcode
///
/// Mutex can be hold by std::unique_lock or std::lock_guard to be sure to be unlocked when leaving the scope
/// \code
///     co_await mtx.lock();
///     std::unique_lock lock(mtx, std::adopt_lock);
/// \endcode
class mutex
{
public:
    /// \brief get the lock, if the lock is already taken lock() will suspend to wait the lock
    func<void> lock()
    {
        if (try_lock())
            co_return;

        co_await _waiting_queue.wait();
    }

    /// \brief get the lock, if the lock is already taken lock() will suspend to wait the lock. The wait period is
    /// controlled by until parameter.
    ///
    /// Usage:
    /// \code
    ///     auto res = co_await mtx.lock(100ms);  // wait up to 100 ms
    ///     if (res == co::timeout) std::cout << "timeout\n";
    /// \endcode
    /// \code
    ///     auto res = co_await mtx.lock(co::this_thread::stop_token());  // wait until stop is requested
    ///     if (res == co::cancel) std::cout << "cancelled\n";
    /// \endcode
    /// \code
    ///     // wait until either 100ms deadline passed or stop is requested
    ///     auto res = co_await mtx.lock(100ms, co::this_thread::stop_token());
    ///     if (res.is_err()) std::cout << "timeout or cancelled\n";
    /// \endcode
    func<result<void>> lock(const co::until& until)
    {
        if (try_lock())
            co_return co::ok();

        co_return co_await _waiting_queue.wait(until);
    }

    /// \brief non blocking version of getting the lock. returns true in case of the lock is obtained
    bool try_lock()
    {
        if (_is_locked)
            return false;

        _is_locked = true;
        return true;
    }

    /// \brief check whether the lock is taken by someone
    [[nodiscard]] bool is_locked() const
    {
        return _is_locked;
    }

    /// \brief return the lock.
    ///
    /// Trying to return the lock from other co::thread is undefined behaviour. Trying to unlock
    /// twice has no effect in Release build but will produce diagnostic in Debug build
    void unlock()
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

    ~mutex()
    {
        assert(!_is_locked);
        assert(_waiting_queue.empty());
    }

private:
    bool _is_locked = false;
    impl::waiting_queue _waiting_queue;
};

}  // namespace co