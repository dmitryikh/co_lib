#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("timed_event notify in advance", "[core]")
{
    co::loop(
        []() -> co::func<void>
        {
            co::timed_event event;
            REQUIRE(event.is_notified() == false);

            REQUIRE(event.notify() == true);
            REQUIRE(event.is_notified() == true);

            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);

            co_await event.wait();
            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);

            co_await event.wait();
            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);

            // all interruptable cases will return (result::is_ok() == true) on already notified event
            auto res = co_await event.wait(100ms);
            REQUIRE(res.is_ok());

            res = co_await event.wait(std::chrono::steady_clock::now() + 100ms);
            REQUIRE(res.is_ok());

            auto stop_source = co::stop_source();
            const auto stop_token = stop_source.get_token();
            res = co_await event.wait(co::until(100ms, stop_token));
            REQUIRE(res.is_ok());

            stop_source.request_stop();
            res = co_await event.wait(co::until(100ms, stop_token));
            REQUIRE(res.is_ok());
        });
}

TEST_CASE("timed_event never notified", "[core]")
{
    co::loop(
        []() -> co::func<void>
        {
            co::timed_event event;

            REQUIRE(event.is_notified() == false);

            // this will lock infinitely
            // co_await event.wait();

            {
                auto start = std::chrono::steady_clock::now();
                auto res = co_await event.wait(100ms);
                REQUIRE(res == co::timeout);
                REQUIRE(event.is_notified() == false);
                // TODO: fix rounding problem
                REQUIRE(std::chrono::steady_clock::now() - start >= 100ms);
            }

            {
                auto deadline = std::chrono::steady_clock::now() + 100ms;
                auto res = co_await event.wait(deadline);
                REQUIRE(res == co::timeout);
                REQUIRE(event.is_notified() == false);
                REQUIRE(std::chrono::steady_clock::now() >= deadline);
            }

            auto stop_source = co::stop_source();
            const auto stop_token = stop_source.get_token();
            // stop is already requested
            stop_source.request_stop();

            {
                auto start = std::chrono::steady_clock::now();
                auto res = co_await event.wait(stop_token);
                REQUIRE(res == co::cancel);
                REQUIRE(event.is_notified() == false);
                REQUIRE(std::chrono::steady_clock::now() - start < 1ms);
            }

            {
                auto start = std::chrono::steady_clock::now();
                auto res = co_await event.wait(co::until(100ms, stop_token));
                REQUIRE(res == co::cancel);
                REQUIRE(event.is_notified() == false);
                REQUIRE(std::chrono::steady_clock::now() - start < 1ms);
            }

            {
                auto stop_source = co::stop_source();
                const auto stop_token = stop_source.get_token();
                // stop will be requested after 50ms
                auto th = co::thread(
                    [&stop_source]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(50ms);
                        stop_source.request_stop();
                    });
                auto start = std::chrono::steady_clock::now();
                auto res = co_await event.wait(co::until(100ms, stop_token));
                REQUIRE(res == co::cancel);
                REQUIRE(event.is_notified() == false);
                REQUIRE(std::chrono::steady_clock::now() - start >= 50ms);
                REQUIRE(std::chrono::steady_clock::now() - start < 100ms);
                co_await th.join();
            }
        });
}

TEST_CASE("timed_event notified concurrently", "[core]")
{
    co::loop(
        []() -> co::func<void>
        {
            {
                co::timed_event event;

                auto th = co::thread(
                    [&event]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(50ms);
                        event.notify();
                    });

                auto start = std::chrono::steady_clock::now();
                co_await event.wait();
                REQUIRE(std::chrono::steady_clock::now() - start >= 50ms);
                co_await th.join();
            }
            {
                co::timed_event event;

                auto th = co::thread(
                    [&event]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(50ms);
                        event.notify();
                    });

                auto start = std::chrono::steady_clock::now();
                auto res = co_await event.wait(25ms);
                REQUIRE(res == co::timeout);
                REQUIRE(std::chrono::steady_clock::now() - start >= 25ms);
                REQUIRE(std::chrono::steady_clock::now() - start < 50ms);
                co_await th.join();
            }
            {
                co::timed_event event;

                auto th = co::thread(
                    [&event]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(50ms);
                        event.notify();
                    });

                auto start = std::chrono::steady_clock::now();
                auto res = co_await event.wait(100ms);
                REQUIRE(res.is_ok());
                REQUIRE(std::chrono::steady_clock::now() - start >= 50ms);
                co_await th.join();
            }
            {
                co::timed_event event;
                auto stop_source = co::stop_source();
                auto stop_token = stop_source.get_token();

                auto th1 = co::thread(
                    [&event]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(50ms);
                        event.notify();
                    });
                auto th2 = co::thread(
                    [&stop_source]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(25ms);
                        stop_source.request_stop();
                    });

                auto start = std::chrono::steady_clock::now();
                auto res = co_await event.wait(co::until(100ms, stop_token));
                REQUIRE(res == co::cancel);
                REQUIRE(std::chrono::steady_clock::now() - start >= 25ms);
                REQUIRE(std::chrono::steady_clock::now() - start < 50ms);
                co_await th1.join();
                co_await th2.join();
            }
            {
                co::timed_event event;
                auto stop_source = co::stop_source();
                auto stop_token = stop_source.get_token();

                auto th1 = co::thread(
                    [&event]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(25ms);
                        event.notify();
                    });
                auto th2 = co::thread(
                    [&stop_source]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(50ms);
                        stop_source.request_stop();
                    });

                auto start = std::chrono::steady_clock::now();
                auto res = co_await event.wait(co::until(100ms, stop_token));
                REQUIRE(res.is_ok());
                REQUIRE(std::chrono::steady_clock::now() - start >= 25ms);
                REQUIRE(std::chrono::steady_clock::now() - start < 50ms);
                co_await th1.join();
                co_await th2.join();
            }
            {
                co::timed_event event;
                auto stop_source = co::stop_source();
                auto stop_token = stop_source.get_token();

                auto th1 = co::thread(
                    [&event]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(50ms);
                        event.notify();
                    });
                auto th2 = co::thread(
                    [&stop_source]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(50ms);
                        stop_source.request_stop();
                    });

                auto start = std::chrono::steady_clock::now();
                auto res = co_await event.wait(co::until(25ms, stop_token));
                REQUIRE(res == co::timeout);
                REQUIRE(std::chrono::steady_clock::now() - start >= 25ms);
                REQUIRE(std::chrono::steady_clock::now() - start < 50ms);
                co_await th1.join();
                co_await th2.join();
            }
        });
}
