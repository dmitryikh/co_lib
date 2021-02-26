#include <catch2/catch.hpp>
#include <co/co.hpp>

class no_copy_no_move
{
public:
    no_copy_no_move(int i)
        : i(i)
    {}

    no_copy_no_move(const no_copy_no_move&) = delete;
    no_copy_no_move(no_copy_no_move&&) = delete;

    no_copy_no_move& operator=(const no_copy_no_move&) = delete;
    no_copy_no_move& operator=(no_copy_no_move&&) = delete;

private:
    int i = 0;
};

class only_move
{
public:
    only_move(int i)
        : i(i)
    {}

    only_move(const only_move&) = delete;
    only_move(only_move&&) = default;

    only_move& operator=(const only_move&) = delete;
    only_move& operator=(only_move&&) = default;

private:
    int i = 0;
};

class only_copy
{
public:
    only_copy(int i)
        : i(i)
    {}

    only_copy(const only_copy&) = default;
    only_copy(only_copy&&) = delete;

    only_copy& operator=(const only_copy&) = default;
    only_copy& operator=(only_copy&&) = delete;

private:
    int i = 0;
};

TEMPLATE_TEST_CASE("result construction", "[primitives]", int, no_copy_no_move, only_move, only_copy)
{
    SECTION("Create with co::err")
    {
        co::result<TestType> res = co::err(co::cancel);
        REQUIRE_THROWS(res.unwrap());
        REQUIRE_NOTHROW(res.err());
    }

    SECTION("Create with co::ok")
    {
        co::result<TestType> res = co::ok(10);
        REQUIRE_NOTHROW(res.unwrap());
        REQUIRE_THROWS(res.err());
    }
}

TEST_CASE("result args construction", "[primitives]")
{
    const std::string msg = "my message";
    co::result<std::string_view> res = co::ok(msg);
    REQUIRE_NOTHROW(res.unwrap());
    REQUIRE_NOTHROW(res.unwrap() == "my message");
}

TEST_CASE("result unwrap", "[primitives]")
{
    auto ptr = std::make_unique<int>(10);
    // TODO: Add deduction guide to write like this
    // co::result res = co::ok(std::move(ptr));
    co::result<std::unique_ptr<int>> res = co::ok(std::move(ptr));

    // Doesn't work on move only objects
    // auto ptr2 = res.unwrap();

    SECTION("move unwrapped")
    {
        auto ptr2 = std::move(res.unwrap());
        REQUIRE(ptr2 != nullptr);
        REQUIRE(*ptr2 == 10);
    }

    SECTION("unwrap moved")
    {
        auto ptr2 = std::move(res).unwrap();
        REQUIRE(ptr2 != nullptr);
        REQUIRE(*ptr2 == 10);
    }
}

TEST_CASE("result void", "[primitives]")
{
    co::result<void> res = co::ok();
    REQUIRE_NOTHROW(res.unwrap());
    res = co::err(co::cancel);
    REQUIRE_THROWS(res.unwrap());

    // Doesn't compile
    // res = co::ok(10);
}
TEST_CASE("result comparison", "[primitives]")
{
    co::result<int> res = co::err(co::cancel);
    REQUIRE(res.is_err() == true);
    REQUIRE(res.is_ok() == false);
    REQUIRE(res.err() == co::cancel);
    REQUIRE(res == co::cancel);
    REQUIRE(res.errc() == co::cancel);
}

TEST_CASE("result error_desc", "[primitives]")
{
    const char* msg = "my favorite error";
    co::result<int> res = co::err(co::timeout, msg);
    REQUIRE(res.is_err() == true);
    REQUIRE(res.what() == msg);
    REQUIRE(res.err() == co::error_desc(co::timeout, msg));
    REQUIRE(res.err() == co::error_desc(co::timeout));
    REQUIRE(res.err() != co::error_desc(co::cancel, msg));
    REQUIRE(res.err() != co::error_desc(co::cancel));
}

TEST_CASE("result can be copied", "[primitives]") {
    {
        co::result<only_copy> res = co::ok(10);
        co::result<only_copy> res2 = res;
    }

    {
        const char* msg = "my favorite error";
        co::result<only_copy> res = co::err(co::cancel, msg);
        co::result<only_copy> res2 = res;
    }
}

TEST_CASE("result can be moved", "[primitives]") {
    {
        co::result<only_move> res = co::ok(10);
        co::result<only_move> res2 = std::move(res);
    }

    {
        const char* msg = "my favorite error";
        co::result<only_copy> res = co::err(co::cancel, msg);
        co::result<only_copy> res2 = std::move(res);
    }
}
