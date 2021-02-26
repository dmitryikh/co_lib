#include <chrono>
#include <catch2/catch.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("until default constructor", "[primitives]")
{
    auto until = co::until();
    REQUIRE(!until.milliseconds().has_value());
    REQUIRE(!until.token().has_value());
}

TEMPLATE_TEST_CASE("until construction with deadline",
                   "[primitives]",
                   std::chrono::steady_clock,
                   std::chrono::system_clock,
                   std::chrono::high_resolution_clock)
{
    const co::stop_token token = co::stop_source().get_token();

    SECTION("with constructor")
    {
        auto until = co::until(TestType::now() + 100ms);
        REQUIRE(until.milliseconds().has_value());
        REQUIRE(!until.token().has_value());
    }
    SECTION("with constructor & stop token")
    {
        auto until = co::until(TestType::now() + 100ms, token);
        REQUIRE(until.milliseconds().has_value());
        REQUIRE(until.token().has_value());
    }
    SECTION("with deadline")
    {
        auto until = co::until::deadline(TestType::now() + 100ms);
        REQUIRE(until.milliseconds().has_value());
        REQUIRE(!until.token().has_value());
    }
    SECTION("with deadline & stop token")
    {
        auto until = co::until::deadline(TestType::now() + 100ms, token);
        REQUIRE(until.milliseconds().has_value());
        REQUIRE(until.token().has_value());
    }
}
TEST_CASE("until construction with timeout", "[primitives]")
{
    const co::stop_token token = co::stop_source().get_token();

    SECTION("with constructor")
    {
        auto until = co::until(100ms);
        REQUIRE(until.milliseconds().has_value());
        REQUIRE(!until.token().has_value());
    }
    SECTION("with constructor & stop token")
    {
        auto until = co::until(100ms, token);
        REQUIRE(until.milliseconds().has_value());
        REQUIRE(until.token().has_value());
    }
    SECTION("with timeout")
    {
        auto until = co::until::timeout(100ms);
        REQUIRE(until.milliseconds().has_value());
        REQUIRE(!until.token().has_value());
    }
    SECTION("with timeout & stop token")
    {
        auto until = co::until::timeout(100ms, token);
        REQUIRE(until.milliseconds().has_value());
        REQUIRE(until.token().has_value());
    }
}

TEST_CASE("until construction with token", "[primitives]")
{
    const co::stop_token token = co::stop_source().get_token();

    SECTION("with constructor")
    {
        auto until = co::until(token);
        REQUIRE(!until.milliseconds().has_value());
        REQUIRE(until.token().has_value());
    }

    SECTION("with cancel")
    {
        auto until = co::until::cancel(token);
        REQUIRE(!until.milliseconds().has_value());
        REQUIRE(until.token().has_value());
    }
}
