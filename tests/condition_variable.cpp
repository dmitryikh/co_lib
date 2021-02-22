#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("condition_variable usage", "[primitives]")
{
    auto start = std::chrono::steady_clock::now();
    co::loop(
        []() -> co::func<void>
        {
            co::condition_variable cv;
            std::string data;
            bool ready = false;

            auto th1 = co::thread(
                [&]() -> co::func<void>
                {
                    co_await co::this_thread::sleep_for(40ms);
                    data = "hello world";
                    ready = true;
                    cv.notify_one();
                },
                "producer");

            auto th2 = co::thread(
                [&]() -> co::func<void>
                {
                    co_await cv.wait([&]() { return ready; }, { 50ms }).unwrap();
                    REQUIRE(data == "hello world");
                },
                "consumer1");
            co_await th1.join();
            co_await th2.join();
        });
    auto end = std::chrono::steady_clock::now();
    REQUIRE(end - start < 50ms);
}
