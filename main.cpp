#include <chrono>
#include <iostream>
#include <variant>
#include <experimental/coroutine>
#include "tmp/generator.hpp"
#include "tmp/eager.hpp"
#include "tmp/lazy.hpp"
#include <co/scheduler.hpp>
#include <co/sleep.hpp>
#include <co/net/network.hpp>

using namespace std::chrono_literals;

co::tmp::eager<int> some_value_fast(int x, int y)
{
    std::cout << "about to return from some_value_fast" << std::endl;
    co_return x + y;
}

co::tmp::eager<int> some_value_throw(int x, int y)
{
    std::cout << "about to return from some_value_throw" << std::endl;
    throw std::runtime_error("bla bla bla");
    co_return x + y;
}

// eager<void> some_value_void(int /*x*/, int /*y*/)
// {
//     co_return;
// }

co::tmp::lazy<int> some_value_lazy(int x, int y)
{
    std::cout << "about to return from some_value_lazy" << std::endl;
    co_return x + y;
}

co::tmp::lazy<void> some_lazy_void(int /*x*/, int /*y*/)
{
    co_return;
}

co::tmp::generator<uint64_t> fibonacci()
{
  uint64_t a = 0, b = 1;
  while (true)
  {
    co_yield b;
    auto tmp = a;
    a = b;
    b += tmp;
  }
}

void usage()
{
    auto gen = fibonacci();
    std::optional<uint64_t> value_opt;
    while ((value_opt = gen.next()))
    {
        if (value_opt.value() > 1'000) break;
        std::cout << value_opt.value() << std::endl;
    }
}

void eager_usage()
{
    co::tmp::eager<int> res = some_value_fast(10, 12);
    std::cout << "res value = " << std::endl;
    std::cout << res.value() << std::endl;

    co::tmp::eager<int> res2 = some_value_throw(12, 14);
    std::cout << "res2 value = " << std::endl;
    // std::cout << res2.value() << std::endl;

    // eager<void> res3 = some_value_void(12, 14);
    // std::cout << "res3 value = " << std::endl;
    // res3.value();
}

void lazy_usage()
{
    co::tmp::lazy<int> res = some_value_lazy(10, 12);
    std::cout << "lazy: res value = " << std::endl;
    std::cout << "lazy: " << res.value() << std::endl;
    // res.value();  will throw

    co::tmp::lazy<void> res2 = some_lazy_void(10, 12);
    std::cout << "lazy: res2 value = " << std::endl;
    res2.value();
}

co::task<void> task10()
{
    for (size_t i = 0; i < 1; i++)
    {
        co_await co::sleep_for(10000ms);
        std::cout << "task10 running\n";
    }
    std::cout << "task10 finished\n";
}

co::task<void> task1()
{
    co::get_scheduler().spawn(task10());
    for (size_t i = 0; i < 10; i++)
    {
        co_await co::sleep_for(1000ms);
        std::cout << "task1 running\n";
    }
    std::cout << "task1 finished\n";
}

co::task<void> task2_nested()
{
    for (size_t i = 0; i < 30; i++)
    {
        co_await co::sleep_for(300ms);
        std::cout << "task2_nested running\n";
    }
    std::cout << "task2_nested finished\n";
    throw std::runtime_error("exceptioN!!");
}

co::task<void> task2()
{
    std::cout << "task2 running\n";
    co_await task2_nested();
}

void scheduler_usage()
{
    auto& scheduler = co::get_scheduler();

    scheduler.spawn(task2());
    scheduler.spawn(task1());
    scheduler.run();
}

co::task<void> client_work(const std::string& ip, uint16_t port)
{
    std::cout << "cobbect\n";
    auto socket = std::make_shared<co::net::tcp>(co_await co::net::connect(ip, port));
    // co_await socket.read_n(&bytes[0], 30);
    co::get_scheduler().spawn([socket] () -> co::task<void>
    {
        for (int i = 0; i < 1; i++)
        {
            const std::string to_write = "abba";
            co_await socket->write(to_write.data(), to_write.size());
            co_await co::sleep_for(1000ms);
            // std::cout << "send done!\n";
        }
        std::cout << "shutdown\n";
        co_await socket->shutdown();
    }());

    while (true)
    {
        std::cout << "read\n";
        std::string bytes(30, 0);
        const size_t len = co_await socket->read(&bytes[0], 30);
        bytes.resize(len);
        if (len == 0)
            break;
        std::cout << "read res: " << bytes << "\n";
    }
}

void net_usage()
{
    auto& scheduler = co::get_scheduler();
    scheduler.spawn(client_work("0.0.0.0", 50007));
    scheduler.run();
}

int main()
{
    // usage();

    // eager_usage();
    // lazy_usage();
    // scheduler_usage();
    net_usage();
}