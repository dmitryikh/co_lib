#include <iostream>
#include <variant>
#include <experimental/coroutine>
#include "generator.hpp"
#include "eager.hpp"
#include "lazy.hpp"


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

int main()
{
    // usage();

    eager_usage();
    // lazy_usage();
}