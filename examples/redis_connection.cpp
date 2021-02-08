#include <co/redis/connection.hpp>
#include <co/redis/client.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

int main()
{
    co::loop([]() -> co::func<void>
    {
        auto conn = (co_await co::redis::connection::connect("127.0.0.1", 6379)).unwrap();
        (co_await conn.write({{"SET", "12", "twelve"}})).unwrap();
        (co_await conn.flush()).unwrap();
        auto repl = co_await conn.read();
        std::cout << repl << std::endl;

        (co_await conn.write({{"GET", "12"}})).unwrap();
        (co_await conn.flush()).unwrap();
        repl = co_await conn.read();
        std::cout << repl << std::endl;

        (co_await conn.write({{"GET", "13"}})).unwrap();
        (co_await conn.flush()).unwrap();
        repl = co_await conn.read();
        std::cout << repl << std::endl;
    }());

    co::loop([]() -> co::func<void>
    {
        auto client = co::redis::client("127.0.0.1", 6379);

        for (int i = 0; i < 100; i++)
        {
            auto f1 = co_await client.set("112", "sun");
            auto f2 = co_await client.get("112");
            co_await client.flush();

            std::cout << i << ": " << co_await f1.get_for(10s) << std::endl;
            std::cout << i << ": " << co_await f2.get_for(10s) << std::endl;

            co_await co::this_thread::sleep_for(300ms);
        }

        client.stop();
        co_await client.join();
    }());
}