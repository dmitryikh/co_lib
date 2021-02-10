#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("co::condition_variable", "[co::condition_variable]")
{
    auto start = std::chrono::steady_clock::now();
    co::loop(
        []() -> co::func<void>
        {
            co::condition_variable cv;
            std::string data;
            bool ready = false;

            auto th1 = co::thread(
                [](auto& cv, auto& data, auto& ready) -> co::func<void>
                {
                    co_await co::this_thread::sleep_for(40ms);
                    data = "hello world";
                    ready = true;
                    cv.notify_one();
                }(cv, data, ready),
                "producer");

            auto th2 = co::thread(
                [](auto& cv, auto& data, auto& ready) -> co::func<void>
                {
                    co_await cv.wait_for(50ms, [&]() { return ready; }).unwrap();
                    std::cout << co::this_thread::get_name() << ": data is ready: " << data << "\n";
                    REQUIRE(data == "hello world");
                }(cv, data, ready),
                "consumer1");
            co_await th1.join();
            co_await th2.join();
        }());
    auto end = std::chrono::steady_clock::now();
    REQUIRE(end - start < 50ms);
}
