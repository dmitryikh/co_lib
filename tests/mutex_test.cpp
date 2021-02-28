#include <mutex>
#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("mutex usage", "[primitives]")
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

TEST_CASE("mutex lock/unlock no contention", "[primitives]")
{
    co::loop(
        []() -> co::func<void>
        {
            co::mutex mutex;
            REQUIRE(mutex.is_locked() == false);
            REQUIRE(mutex.try_lock() == true);
            REQUIRE(mutex.is_locked() == true);
            REQUIRE(mutex.try_lock() == false);
            mutex.unlock();
            REQUIRE(mutex.is_locked() == false);
            co_await mutex.lock();
            REQUIRE(mutex.is_locked() == true);
            mutex.unlock();

            {
                auto res = co_await mutex.lock(co::until(10ms));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }

            {
                auto res = co_await mutex.lock(co::until(std::chrono::steady_clock::now() + 10ms));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }

            REQUIRE((co_await mutex.lock(co::until(std::chrono::system_clock::now() + 10ms))).is_ok() == true);
            REQUIRE(mutex.is_locked() == true);
            mutex.unlock();

            auto stop_source = co::stop_source();
            auto token = stop_source.get_token();
            {
                auto res = co_await mutex.lock(co::until(token));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }

            {
                auto res = co_await mutex.lock(co::until(10ms, token));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }

            {
                auto res = co_await mutex.lock(co::until(std::chrono::steady_clock::now() + 10ms, token));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }

            stop_source.request_stop();
            {
                // stop is requested but we don't need to await to get lock
                auto res = co_await mutex.lock(co::until(token));
                REQUIRE(res.is_ok());
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }
        });
}

TEST_CASE("mutex lock/unlock with contention", "[primitives]")
{
    co::loop(
        []() -> co::func<void>
        {
            co::mutex mutex;
            auto rival_thread1 = co::thread(co::invoke(
                [&]() -> co::func<void>
                {
                    while (!co::this_thread::stop_requested())
                    {
                        co_await mutex.lock();
                        co_await co::this_thread::sleep_for(3ms);
                        mutex.unlock();
                    }
                }));

            auto rival_thread2 = co::thread(co::invoke(
                [&]() -> co::func<void>
                {
                    while (!co::this_thread::stop_requested())
                    {
                        co_await mutex.lock();
                        co_await co::this_thread::sleep_for(3ms);
                        mutex.unlock();
                    }
                }));

            // give the control to another threads to start using the mutex
            co_await co::this_thread::sleep_for(2ms);
            REQUIRE(mutex.is_locked() == true);
            REQUIRE(mutex.try_lock() == false);
            co_await co::this_thread::sleep_for(5ms);
            REQUIRE(mutex.try_lock() == false);
            {
                co_await mutex.lock();
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }
            {
                auto res = co_await mutex.lock(co::until(10ms));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }
            {
                auto res = co_await mutex.lock(co::until(std::chrono::steady_clock::now() + 10ms));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }

            REQUIRE((co_await mutex.lock(co::until(std::chrono::system_clock::now() + 10ms))).is_ok() == true);
            REQUIRE(mutex.is_locked() == true);
            mutex.unlock();

            auto stop_source = co::stop_source();
            const auto token = stop_source.get_token();
            {
                auto res = co_await mutex.lock(co::until(token));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }
            {
                auto res = co_await mutex.lock(co::until(10ms, token));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }
            {
                auto res = co_await mutex.lock(co::until(std::chrono::steady_clock::now() + 10ms, token));
                REQUIRE(res.is_ok() == true);
                REQUIRE(mutex.is_locked() == true);
                mutex.unlock();
            }

            stop_source.request_stop();
            {
                auto res = co_await mutex.lock(co::until(token));
                REQUIRE(res == co::cancel);
            }

            rival_thread1.request_stop();
            rival_thread2.request_stop();
            co_await rival_thread1.join();
            co_await rival_thread2.join();
        });
}

TEST_CASE("mutex adopt", "[primitives]")
{

    co::loop(
        []() -> co::func<void>
        {
            co::mutex mutex;
            {
                co_await mutex.lock();
                REQUIRE(mutex.is_locked() == true);
                std::lock_guard lock(mutex, std::adopt_lock);
            }
            REQUIRE(mutex.is_locked() == false);
            {
                co_await mutex.lock();
                REQUIRE(mutex.is_locked() == true);
                std::unique_lock lock(mutex, std::adopt_lock);
            }
            REQUIRE(mutex.is_locked() == false);
        });
}