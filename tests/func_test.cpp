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

TEST_CASE("func move only result")
{
    auto func = []() -> co::func<std::unique_ptr<int>> { co_return std::make_unique<int>(10); };

    co::loop(
        [&]() -> co::func<void>
        {
            auto res = co_await func();
            REQUIRE(res != nullptr);
            REQUIRE(*res == 10);
        });
}

TEST_CASE("func exception")
{
    auto func = []() -> co::func<int>
    {
        throw std::runtime_error("exception is here");
        co_return 10;
    };

    auto func_void = []() -> co::func<void>
    {
        throw std::logic_error("exception is here");
        co_return;
    };

    co::loop(
        [&]() -> co::func<void>
        {
            REQUIRE_THROWS_AS(co_await func(), std::runtime_error);
            REQUIRE_THROWS_AS(co_await func_void(), std::logic_error);
        });
}

TEST_CASE("func unwrap")
{
    auto func_ok = []() -> co::func<co::result<int>> { co_return co::ok(10); };

    auto func_err = []() -> co::func<co::result<int>> { co_return co::err(co::other); };

    co::loop(
        [&]() -> co::func<void>
        {
            REQUIRE_NOTHROW(co_await func_ok().unwrap());
            REQUIRE_NOTHROW((co_await func_ok()).unwrap());
            REQUIRE_THROWS_AS(co_await func_err().unwrap(), co::exception);
            REQUIRE_THROWS_AS((co_await func_err()).unwrap(), co::exception);
        });
}
