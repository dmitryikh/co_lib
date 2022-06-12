#pragma once

#include <co/func.hpp>
#include <co/stop_token.hpp>
#include <co/thread.hpp>

namespace co
{

template <typename T1, typename T2>
co::func<std::tuple<T1, T2>> when_any(co::func<T1>&& f1, co::func<T2>&& f2, co::stop_source& stop)
{
    co::event finish;
    // need to use optional here because T1,2 could be not default constructable
    std::optional<T1> res1;
    std::optional<T2> res2;
    auto l1 = [&res1, &finish](co::func<T1>&& f1) -> co::func<void>
    {
        res1 = co_await f1;
        finish.notify();
    };
    auto th1 = co::thread(l1(std::move(f1)));

    auto l2 = [&res2, &finish](co::func<T2>&& f2) -> co::func<void>
    {
        res2 = co_await f2;
        finish.notify();
    };
    auto th2 = co::thread(l2(std::move(f2)));

    auto _ = co_await finish.wait(stop.get_token());

    stop.request_stop();
    co_await th1.join();
    co_await th2.join();

    CO_DCHECK(res1.has_value() && res2.has_value());

    co_return { std::move(res1.value()), std::move(res2.value()) };
}

template <typename T1, typename T2>
co::func<std::tuple<T1, T2>> when_all(co::func<T1>&& f1, co::func<T2>&& f2, const co::stop_token& token)
{
    co::event finish1;
    co::event finish2;
    std::tuple<T1, T2> res;
    auto l1 = [&res, &finish1](co::func<T1>&& f1) -> co::func<void>
    {
        std::get<0>(res) = co_await f1;
        finish1.notify();
    };
    auto th1 = co::thread(l1(std::move(f1)));

    auto l2 = [&res, &finish2](co::func<T2>&& f2) -> co::func<void>
    {
        std::get<1>(res) = co_await f2;
        finish2.notify();
    };
    auto th2 = co::thread(l2(std::move(f2)));

    auto _ = co_await finish1.wait(token);
    _ = co_await finish2.wait(token);

    co_await th1.join();
    co_await th2.join();

    co_return res;
}

}  // namespace co
