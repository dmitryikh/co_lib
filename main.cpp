#include <chrono>
#include <iostream>
#include <variant>
#include <experimental/coroutine>
#include "tmp/generator.hpp"
#include "tmp/eager.hpp"
#include "tmp/lazy.hpp"
#include <co/scheduler.hpp>
#include <co/sleep.hpp>
#include <co/mutex.hpp>
#include <co/net/network.hpp>
#include <co/thread.hpp>
#include <co/co.hpp>

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
        co_await co::this_thread::sleep_for(3s);
        std::cout << "task10 running\n";
    }
    std::cout << "task10 finished\n";
}

co::task<void> task1()
{
    co::thread(task10()).detach();
    for (size_t i = 0; i < 3; i++)
    {
        co_await co::this_thread::sleep_for(1s);
        std::cout << "task1 running\n";
    }
    std::cout << "task1 finished\n";
}

co::task<void> task2_nested()
{
    for (size_t i = 0; i < 10; i++)
    {
        co_await co::this_thread::sleep_for(300ms);
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
    co::loop([] () -> co::task<void> 
    {
        co::thread(task2()).detach();
        co::thread(task1()).detach();
        co_return;
    }());
}

co::task<void> client_work(const std::string& ip, uint16_t port)
{
    auto socket = std::make_shared<co::net::tcp>(co_await co::net::connect(ip, port));
    // co_await socket.read_n(&bytes[0], 30);
    co::thread([socket] () -> co::task<void>
    {
        for (int i = 0; i < 3; i++)
        {
            const std::string to_write = "abba" + std::to_string(i);
            co_await socket->write(to_write.data(), to_write.size());
            co_await co::this_thread::sleep_for(1000ms);
        }
        std::cout << "shutdown\n";
        co_await socket->shutdown();
    }(), "writer").detach();

    while (true)
    {
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
    co::loop(client_work("0.0.0.0", 50007));
}

void mutex_usage()
{
    co::mutex mutex;
    co::loop([&]() -> co::task<void>
    {
        co::thread([&]() -> co::task<void>
        {
            std::cout << "task0 about to get lock\n";
            auto scoped_lock = co_await mutex.lock();
            std::cout << "task0 got lock\n";
            co_await co::this_thread::sleep_for(1s);
            std::cout << "task0 release lock\n";
        }()).detach();
        co::thread([&]() -> co::task<void>
        {
            auto scoped_lock = co_await mutex.lock();
            std::cout << "task1 got lock\n";
            co_await co::this_thread::sleep_for(1s);
            std::cout << "task1 release lock\n";
        }()).detach();
        auto th = co::thread([&]() -> co::task<void>
        {
            auto scoped_lock = co_await mutex.lock();
            std::cout << "task2 got lock\n";
            co_await co::this_thread::sleep_for(1s);
            std::cout << "task2 release lock\n";
            co_return;

        }());
        co_await th.join();
        co_return;
    }());
}

int main()
{
    // usage();

    // eager_usage();
    // lazy_usage();
    scheduler_usage();
    net_usage();
    mutex_usage();
}