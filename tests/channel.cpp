#include <catch2/catch.hpp>
#include <co/channel.hpp>
#include <co/ts/channel.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEMPLATE_TEST_CASE("channel usage", "[primitives]", co::channel<int>, co::ts::channel<int>)
{
    auto start = std::chrono::steady_clock::now();
    co::loop(
        []() -> co::func<void>
        {
            TestType ch(3);
            auto th1 = co::thread(
                [ch]() mutable -> co::func<void>
                {
                    for (int i = 0; i < 10; i++)
                    {
                        co_await ch.push(i).unwrap();
                    }
                    ch.close();
                },
                "producer");

            auto th2 = co::thread(
                [ch]() mutable -> co::func<void>
                {
                    while (true)
                    {
                        auto val = co_await ch.pop();
                        if (val == co::closed)
                            break;
                        co_await co::this_thread::sleep_for(10ms);
                    }
                },
                "consumer1");

            auto th3 = co::thread(
                [ch]() mutable -> co::func<void>
                {
                    while (true)
                    {
                        auto val = co_await ch.pop();
                        if (val == co::closed)
                            break;
                        co_await co::this_thread::sleep_for(5ms);
                    }
                },
                "consumer2");

            co_await th1.join();
            co_await th2.join();
            co_await th3.join();
        }());
    auto end = std::chrono::steady_clock::now();
    REQUIRE(end - start > 20ms);
    // TODO: sometimes it runs out of time..
    REQUIRE(end - start < 50ms + 5ms);
}

TEST_CASE("channel simple usage", "[primitives]")
{
    co::loop(
        []() -> co::func<void>
        {
            const size_t capacity = 2;
            co::channel<int> ch(capacity);

            auto th1 = co::thread(
                [ch]() mutable -> co::func<void>
                {
                    co_await ch.push(1).unwrap();
                    co_await ch.push(2).unwrap();
                    co_await ch.push(3).unwrap();
                    ch.close();
                },
                "producer");

            while (true)
            {
                auto val = co_await ch.pop();
                if (val == co::closed)
                    break;
            }
            co_await th1.join();
        }());
}
