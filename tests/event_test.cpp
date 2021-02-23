#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("event usage", "[core]")
{
    std::vector<co::event> events(1000);
    co::loop(
        [&events]() -> co::func<void>
        {
            for (auto& event : events)
            {
                co::thread(
                    [&event]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(2ms);
                        event.notify();
                        co_return;
                    });
            }
            for (auto& event : events)
            {
                co::thread([&event]() -> co::func<void> { co_await event.wait(); });
            }
            co_return;
        });

    for (auto& event : events)
    {
        REQUIRE(event.is_notified());
    }
}
TEST_CASE("event wait/notify", "[core]")
{
    co::loop(
        []() -> co::func<void>
        {
            co::event event;
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
            co_return;
        });

    co::loop(
        []() -> co::func<void>
        {
            co::event event;
            bool notified = false;
            auto th = co::thread(
                [&event, &notified]() -> co::func<void>
                {
                    REQUIRE(event.is_notified() == false);

                    REQUIRE(event.notify() == true);
                    REQUIRE(event.is_notified() == true);
                    notified = true;

                    REQUIRE(event.notify() == false);
                    REQUIRE(event.is_notified() == true);
                    co_return;
                });

            co_await event.wait();
            REQUIRE(notified == true);
            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);

            co_await event.wait();
            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);
            co_await th.join();
        });
}
