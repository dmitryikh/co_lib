#pragma once

#include <co/func.hpp>
#include <co/thread.hpp>
#include <co/stop_token.hpp>

namespace co
{

template <typename T1, typename T2>
co::func<std::tuple<T1, T2>> when_any(co::func<T1>&& f1, co::func<T2>&& f2, co::stop_source& stop)
{
    co::event finish;
    std::tuple<T1, T2> res;
    auto l1 = [&res, &finish] (co::func<T1>&& f1) -> co::func<void>
    {
        std::get<0>(res) = co_await f1;
        finish.notify();
    };
    auto th1 = co::thread(l1(std::move(f1)));

    auto l2 = [&res, &finish] (co::func<T2>&& f2) -> co::func<void>
    {
        std::get<1>(res) = co_await f2;
        finish.notify();
    };
    auto th2 = co::thread(l2(std::move(f2)));

    co_await finish.wait(stop.get_token());

    stop.request_stop();
    co_await th1.join();
    co_await th2.join();

    co_return res;
}

template <typename T1, typename T2>
co::func<std::tuple<T1, T2>> when_all(co::func<T1>&& f1, co::func<T2>&& f2, const co::stop_token& token)
{
    co::event finish1;
    co::event finish2;
    std::tuple<T1, T2> res;
    auto l1 = [&res, &finish1] (co::func<T1>&& f1) -> co::func<void>
    {
        std::get<0>(res) = co_await f1;
        finish1.notify();
    };
    auto th1 = co::thread(l1(std::move(f1)));

    auto l2 = [&res, &finish2] (co::func<T2>&& f2) -> co::func<void>
    {
        std::get<1>(res) = co_await f2;
        finish2.notify();
    };
    auto th2 = co::thread(l2(std::move(f2)));

    co_await finish1.wait(token);
    co_await finish2.wait(token);

    co_await th1.join();
    co_await th2.join();

    co_return res;
}

}  // namespace co
