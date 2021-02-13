#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("co::event::wait", "[base]")
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
