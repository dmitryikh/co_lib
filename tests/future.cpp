#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("future usage", "[primitives]")
{
    co::loop(
        []() -> co::func<void>
        {
            co::promise<std::string> promise;
            auto future = promise.get_future();

            co::thread(
                [promise = std::move(promise)]() mutable -> co::func<void>
                {
                    co_await co::this_thread::sleep_for(50ms);
                    promise.set_value("hello world");
                })
                .detach();

            REQUIRE(co_await future.get({ 100ms }).unwrap() == "hello world");
        });
}
