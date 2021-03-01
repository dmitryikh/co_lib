#pragma once

#include <chrono>
#include <optional>
#include <variant>
#include <co/stop_token.hpp>

namespace co
{

namespace impl
{

/// \brief converts time point from Clock to TargetClock
template <typename TargetClock, typename Clock, typename Duration>
typename TargetClock::time_point time_point_conv(const std::chrono::time_point<Clock, Duration>& time)
{
    // original code are taken from
    // https://github.com/facebook/folly/blob/master/folly/detail/Futex-inl.h

    using time_point_type = std::chrono::time_point<Clock, Duration>;
    using target_duration_type = typename TargetClock::duration;
    using target_time_point = typename TargetClock::time_point;

    if (time == time_point_type::max())
        return target_time_point::max();

    if constexpr (std::is_same_v<Clock, TargetClock>)
    {
        return std::chrono::time_point_cast<target_duration_type>(time);
    }
    else
    {
        // different clocks
        auto const delta = time - Clock::now();
        return TargetClock::now() + std::chrono::duration_cast<target_duration_type>(delta);
    }
}

}  // namespace impl

/// \brief co::until is an immutable object representing interruption conditions for awaitable task.
///
/// Two types of interruptions are supported. Can be combined.
///   * timeout or deadline, represented by std::chrono::time_point and std::chrono::duration respectively
///   * co::stop_token, representing external stop signal
///
/// Usage:
/// \code
///     co_await foo(co::until::cancel(co::this_thread::stop_token()));
///     co_await foo(co::this_thread::stop_token());  // implicit short form
///
///     co_await foo(co::until::timeout(100ms));
///     co_await foo(100ms);  // implicit short form
///
///     co_await foo(co::until::deadline(std::chrono::steady_clock::now() + 100ms));
///     co_await foo(std::chrono::steady_clock::now() + 100ms);  // implicit short form
///
///     // combination of timeout & stop signal
///     co_await foo(co::until::timeout(100ms, co::this_thread::stop_token()));
///
///     // shorter version, using until constructors
///     co_await foo(co::until(100ms, co::this_thread::stop_token()));
/// \endcode
///
class until
{
    using clock_type = std::chrono::steady_clock;
    using time_type = std::chrono::steady_clock::time_point;
    using duration_type = clock_type::duration;
    using deadline_variants = std::variant<std::monostate, time_type, duration_type>;

public:
    /// \brief default constructor, set no timeout, no deadline, and no stop signal
    until() = default;

    /// \brief set only stop token
    until(co::stop_token token)  // NOLINT(google-explicit-constructor)
        : _token(std::move(token))
    {}

    /// \brief set only timeout
    template <class Rep, class Period>
    until(std::chrono::duration<Rep, Period> timeout)  // NOLINT(google-explicit-constructor)
        : _deadline(std::chrono::duration_cast<duration_type>(timeout))
    {}

    /// \brief set timeout & stop token
    template <class Rep, class Period>
    until(std::chrono::duration<Rep, Period> timeout, co::stop_token token)
        : _deadline(std::chrono::duration_cast<duration_type>(timeout))
        , _token(std::move(token))
    {}

    /// \brief set deadline
    template <class Clock, class Duration>
    until(std::chrono::time_point<Clock, Duration> deadline)  // NOLINT(google-explicit-constructor)
        : _deadline(impl::time_point_conv<clock_type>(deadline))
    {}

    /// \brief set deadline & stop token
    template <class Clock, class Duration>
    until(std::chrono::time_point<Clock, Duration> deadline, co::stop_token token)
        : _deadline(impl::time_point_conv<clock_type>(deadline))
        , _token(std::move(token))
    {}

    /// \brief static method to explicitly create an object with only deadline set
    template <class Clock, class Duration>
    static until deadline(std::chrono::time_point<Clock, Duration> deadline)
    {
        return until{ deadline };
    }

    /// \brief static method to explicitly create an object with deadline & stop token
    template <class Clock, class Duration>
    static until deadline(std::chrono::time_point<Clock, Duration> deadline, co::stop_token token)
    {
        return until{ deadline, std::move(token) };
    }

    /// \brief static method to explicitly create an object with only timeout set
    template <class Rep, class Period>
    static until timeout(std::chrono::duration<Rep, Period> timeout)
    {
        return until{ timeout };
    }

    /// \brief static method to explicitly create an object with timeout & stop token
    template <class Rep, class Period>
    static until timeout(std::chrono::duration<Rep, Period> timeout, co::stop_token token)
    {
        return until{ timeout, std::move(token) };
    }

    /// \brief static method to explicitly create an object with only stop token set
    static until cancel(co::stop_token token)
    {
        return until{ std::move(token) };
    }

    /// \brief get the token, std::nullopt otherwise
    [[nodiscard]] const std::optional<co::stop_token>& token() const
    {
        return _token;
    }

    /// \brief get the token, std::nullopt otherwise
    std::optional<co::stop_token>& token()
    {
        return _token;
    }

    /// \brief returns number of milliseconds until cancellation, if timeout or deadline are set.
    ///
    /// returns std::nullopt otherwise
    [[nodiscard]] std::optional<int64_t> milliseconds() const
    {
        if (std::holds_alternative<time_type>(_deadline))
        {
            const auto time_point = std::get<time_type>(_deadline);
            return std::chrono::round<std::chrono::milliseconds>(time_point - clock_type ::now()).count();
        }
        else if (std::holds_alternative<duration_type>(_deadline))
        {
            const auto duration = std::get<duration_type>(_deadline);
            return std::chrono::round<std::chrono::milliseconds>(duration).count();
        }
        return std::nullopt;
    }

private:
    deadline_variants _deadline;
    std::optional<co::stop_token> _token;
};

}  // namespace co