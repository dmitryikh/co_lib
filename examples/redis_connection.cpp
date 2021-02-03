#include <co/redis/connection.hpp>
#include <co/co.hpp>


int main()
{
    co::loop([]() -> co::func<void>
    {
        auto conn = co_await co::redis::connection::connect("127.0.0.1", 6379);
        co_await conn.write({{"SET", "12", "twelve"}});
        co_await conn.flush();
        auto repl = co_await conn.read();
        std::cout << repl.value() << std::endl;

        co_await conn.write({{"GET", "12"}});
        co_await conn.flush();
        repl = co_await conn.read();
        std::cout << repl.value() << std::endl;

        co_await conn.write({{"GET", "13"}});
        co_await conn.flush();
        repl = co_await conn.read();
        std::cout << repl.value() << std::endl;
    }());
}