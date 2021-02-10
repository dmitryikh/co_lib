#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("co::channel", "[co::channel]")
{
    auto start = std::chrono::steady_clock::now();
    co::loop(
        []() -> co::func<void>
        {
            co::channel<int> ch(3);
            auto th1 = co::thread(
                [](auto& ch) -> co::func<void>
                {
                    for (int i = 0; i < 10; i++)
                    {
                        co_await ch.push(i).unwrap();
                    }
                    ch.close();
                }(ch),
                "producer");

            auto th2 = co::thread(
                [](auto& ch) -> co::func<void>
                {
                    while (true)
                    {
                        auto val = co_await ch.pop();
                        if (val == co::closed)
                            break;
                        co_await co::this_thread::sleep_for(10ms);
                    }
                }(ch),
                "consumer1");

            auto th3 = co::thread(
                [](auto& ch) -> co::func<void>
                {
                    while (true)
                    {
                        auto val = co_await ch.pop();
                        if (val == co::closed)
                            break;
                        co_await co::this_thread::sleep_for(5ms);
                    }
                }(ch),
                "consumer2");

            co_await th1.join();
            co_await th2.join();
            co_await th3.join();
        }());
    auto end = std::chrono::steady_clock::now();
    REQUIRE(end - start > 20ms);
    REQUIRE(end - start < 45ms);
}
