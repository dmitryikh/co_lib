#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("co::mutex", "[co::mutex]")
{
    auto start = std::chrono::steady_clock::now();
    co::loop(
        []() -> co::func<void>
        {
            co::mutex mutex;
            auto th1 = co::thread(
                [&]() -> co::func<void>
                {
                    co_await mutex.lock();
                    co_await co::this_thread::sleep_for(11ms);
                    mutex.unlock();
                });
            auto th2 = co::thread(
                [&]() -> co::func<void>
                {
                    co_await mutex.lock();
                    co_await co::this_thread::sleep_for(11ms);
                    mutex.unlock();
                });
            auto th3 = co::thread(
                [&]() -> co::func<void>
                {
                    while (co_await mutex.lock(2ms) == co::timeout) {}
                    co_await co::this_thread::sleep_for(11ms);
                    mutex.unlock();
                });
            co_await th1.join();
            co_await th2.join();
            co_await th3.join();
            co_return;
        });

    auto end = std::chrono::steady_clock::now();
    REQUIRE(end - start >= 30ms);
}
