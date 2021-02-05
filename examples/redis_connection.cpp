#include <co/redis/connection.hpp>
#include <co/redis/client.hpp>
#include <co/co.hpp>


int main()
{
    co::loop([]() -> co::func<void>
    {
        auto conn = (co_await co::redis::connection::connect("127.0.0.1", 6379)).unwrap();
        co_await conn.write({{"SET", "12", "twelve"}});
        co_await conn.flush();
        auto repl = co_await conn.read();
        std::cout << repl << std::endl;

        co_await conn.write({{"GET", "12"}});
        co_await conn.flush();
        repl = co_await conn.read();
        std::cout << repl << std::endl;

        co_await conn.write({{"GET", "13"}});
        co_await conn.flush();
        repl = co_await conn.read();
        std::cout << repl << std::endl;
    }());

    co::loop([]() -> co::func<void>
    {
        auto client = co::redis::client("127.0.0.1", 6379);

        auto f1 = client.set("112", "sun");
        auto f2 = client.get("112");
        auto f3 = client.get("113");

        std::cout << co_await f1.get() << std::endl;
        std::cout << co_await f2.get() << std::endl;
        std::cout << co_await f3.get() << std::endl;

        client.stop();
        co_await client.join();
    }());
}