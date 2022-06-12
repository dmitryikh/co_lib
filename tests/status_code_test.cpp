#include <catch2/catch.hpp>
#include <co/status_code.hpp>

using namespace std::string_literals;

enum class my_code
{
    cancel = 1,
    timeout = 2,
    broken = 3,
    other = 4
};

enum class their_code
{
    cancel = 1,
    timeout = 2,
    broken = 3,
    other = 4
};

class my_category : public co::status_category
{
    constexpr static uint64_t id = 0x47030a620c347160;

public:
    my_category()
        : co::status_category(id)
    {}

    const char* name() const noexcept override
    {
        return "my_category";
    }
    const char* message(int status) const noexcept override
    {
        switch (static_cast<my_code>(status))
        {
        case my_code::cancel:
            return "cancel";
        case my_code::timeout:
            return "timeout";
        case my_code::broken:
            return "broken";
        case my_code::other:
            return "other";
        }
        CO_CHECK(false);
        return "undefined";
    }
};

class their_category : public co::status_category
{
    constexpr static uint64_t id = 0xd2a1cc611159e464;

public:
    their_category()
        : co::status_category(id)
    {}

    const char* name() const noexcept override
    {
        return "their_category";
    }
    const char* message(int status) const noexcept override
    {
        switch (static_cast<their_code>(status))
        {
        case their_code::cancel:
            return "cancel t";
        case their_code::timeout:
            return "timeout t";
        case their_code::broken:
            return "broken t";
        case their_code::other:
            return "other t";
        }
        CO_CHECK(false);
        return "undefined";
    }
};

inline co::status_code make_status_code(my_code e)
{
    const static my_category global_my_category;
    return co::status_code{ e, &global_my_category };
}

inline co::status_code make_status_code(their_code e)
{
    const static their_category global_their_category;
    return co::status_code{ e, &global_their_category };
}

TEST_CASE("status category", "[core]")
{
    co::status_code code = my_code::cancel;
    co::status_code code2 = my_code::cancel;
    co::status_code code3 = my_code::timeout;
    co::status_code code4 = their_code::cancel;
    REQUIRE(code == code2);
    REQUIRE(code != code3);
    REQUIRE(code != code4);
    REQUIRE(code3 != code4);

    REQUIRE(code == my_code::cancel);
    REQUIRE(code != my_code::timeout);
    REQUIRE(code != their_code::cancel);
    REQUIRE(code != their_code::timeout);
    REQUIRE(code.message() == "cancel"s);
    REQUIRE(code.category_name() == "my_category"s);

    REQUIRE(code3 != my_code::cancel);
    REQUIRE(code3 == my_code::timeout);
    REQUIRE(code3 != their_code::cancel);
    REQUIRE(code3 != their_code::timeout);
    REQUIRE(code3.message() == "timeout"s);
    REQUIRE(code3.category_name() == "my_category"s);

    REQUIRE(code4 != my_code::cancel);
    REQUIRE(code4 != my_code::timeout);
    REQUIRE(code4 == their_code::cancel);
    REQUIRE(code4 != their_code::timeout);
    REQUIRE(code4.message() == "cancel t"s);
    REQUIRE(code4.category_name() == "their_category"s);
}