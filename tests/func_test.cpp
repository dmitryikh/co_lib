#include <catch2/catch.hpp>
#include <co/co.hpp>

TEST_CASE("invoke usage", "[core]")
{
    auto i_ptr = std::make_unique<int>(42);
    auto func = co::invoke(
        [](auto i_ptr) -> co::func<void>
        {
            REQUIRE(i_ptr != nullptr);
            REQUIRE(*i_ptr == 42);
            co_return;
        },
        std::move(i_ptr));
    i_ptr.reset();
    co::loop(std::move(func));

    const std::string str = "hello world";
    auto func2 = co::invoke(
        [&str]() -> co::func<void>
        {
            REQUIRE(str == "hello world");
            co_return;
        });
    co::loop(std::move(func2));
}
