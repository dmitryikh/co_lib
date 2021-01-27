#include <chrono>
#include <iostream>
#include <variant>
#include <experimental/coroutine>
#include "generator.hpp"
#include "eager.hpp"
#include "lazy.hpp"
#include "scheduler.hpp"
#include "sleep.hpp"

using namespace std::chrono_literals;

eager<int> some_value_fast(int x, int y)
{
    std::cout << "about to return from some_value_fast" << std::endl;
    co_return x + y;
}

eager<int> some_value_throw(int x, int y)
{
    std::cout << "about to return from some_value_throw" << std::endl;
    throw std::runtime_error("bla bla bla");
    co_return x + y;
}

// eager<void> some_value_void(int /*x*/, int /*y*/)
// {
//     co_return;
// }

lazy<int> some_value_lazy(int x, int y)
{
    std::cout << "about to return from some_value_lazy" << std::endl;
    co_return x + y;
}

lazy<void> some_lazy_void(int /*x*/, int /*y*/)
{
    co_return;
}

generator<uint64_t> fibonacci()
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
    eager<int> res = some_value_fast(10, 12);
    std::cout << "res value = " << std::endl;
    std::cout << res.value() << std::endl;

    eager<int> res2 = some_value_throw(12, 14);
    std::cout << "res2 value = " << std::endl;
    // std::cout << res2.value() << std::endl;

    // eager<void> res3 = some_value_void(12, 14);
    // std::cout << "res3 value = " << std::endl;
    // res3.value();
}

void lazy_usage()
{
    lazy<int> res = some_value_lazy(10, 12);
    std::cout << "lazy: res value = " << std::endl;
    std::cout << "lazy: " << res.value() << std::endl;
    // res.value();  will throw

    lazy<void> res2 = some_lazy_void(10, 12);
    std::cout << "lazy: res2 value = " << std::endl;
    res2.value();
}

task<void> task10()
{
    for (size_t i = 0; i < 1; i++)
    {
        co_await sleep_for(10000ms);
        std::cout << "task10 running\n";
    }
    std::cout << "task10 finished\n";
}

task<void> task1()
{
    get_scheduler().spawn(task10());
    for (size_t i = 0; i < 10; i++)
    {
        co_await sleep_for(1000ms);
        std::cout << "task1 running\n";
    }
    std::cout << "task1 finished\n";
}

task<void> task2_nested()
{
    for (size_t i = 0; i < 30; i++)
    {
        co_await sleep_for(300ms);
        std::cout << "task2_nested running\n";
    }
    std::cout << "task2_nested finished\n";
    throw std::runtime_error("exceptioN!!");
}

task<void> task2()
{
    std::cout << "task2 running\n";
    co_await task2_nested();
}

void scheduler_usage()
{
    auto& scheduler = get_scheduler();

    scheduler.spawn(task2());
    scheduler.spawn(task1());
    scheduler.run();
}

int main()
{
    // usage();

    // eager_usage();
    // lazy_usage();
    scheduler_usage();
}