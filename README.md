# co_lib

co_lib is an experimental asynchronous C++20 framework that feels like std library.

co_lib design priorities:
1. Reuse common patterns and abstractions
2. Make it hard to misuse
3. Performance also matters

co_lib benefits:
1. Cancellation as a first class citizen: co::stop_token, co::stop_source, almost all awaited ops can be cancelled.
2. co::result<T> type (like in Rust) to check the result of an operation
3. based on libuv C library: even loop, network, timers, etc..

Current limitations:
1. Single threaded
2. No communications between event loop thread and other system threads
3. Developing and testing on clang 11 only

# Dependencies
1. C++ 20
2. boost 1.75
3. libuv 1.40
4. conan package manager

# Examples

Have a look to [examples/introduction.cpp](examples/introduction.cpp) as an initial guide to the library;

```cpp
#include <chrono>
#include <co/co.hpp>

using namespace std::chrono_literals;

co::func<void> receiver(co::channel<std::string> ch)
{
    while (true)
    {
        co::result<std::string> res = co_await ch.pop();
        if (res == co::closed)
            break;

        std::cout << co::this_thread::name() << ": receive '" << res.unwrap() << "'\n";
    }
    std::cout << co::this_thread::name() << ": stop receiving\n";
}

int main()
{
    co::loop(
        []() -> co::func<void>
        {
            const size_t buffer_capacity = 2;
            co::channel<std::string> ch(buffer_capacity);

            co::thread(receiver(ch), "receiver").detach();

            auto sender = co::thread(
                [ch]() mutable -> co::func<void>
                {
                    try
                    {
                        const auto token = co::this_thread::stop_token();
                        while (true)
                        {
                            co_await co::this_thread::sleep_for(1s, token).unwrap();
                            co_await ch.push("hello world", token).unwrap();
                            std::cout << co::this_thread::name() << ": send a value\n";
                        }
                    }
                    catch (const co::exception& coexc)
                    {
                        std::cout << co::this_thread::name() << ": stop sendings - " << coexc << "\n";
                    }
                    ch.close();
                },
                "sender");

            co_await co::this_thread::sleep_for(5s);
            sender.request_stop();
            std::cout << co::this_thread::name() << ": stop request sent\n";
            co_await sender.join();
        });
}
```

# Build Instruction
`co_lib` is header only library. It uses conan package manager to manage its
dependecies and also to be included into other projects.

To build the package locally:

```bash
conan create . -tf conan/test_package
```

To build example executables using `co_lib/0.1` package:
```bash
cd examples
mkdir build && cd build
conan install .. --build=missing -s build_type=Debug
cmake -DCMAKE_BUILD_TYPE=Debug  ..
cmake --build . -j 4
```

# Tests
Tests are run automatically while `co_lib` conan package is creating. To
build & run tests manually:

```bash
mkdir build && cd build
conan install .. --build=missing -s build_type=Debug
cmake -DCMAKE_BUILD_TYPE=Debug  ..
cmake --build . -j 4
ctest --build-config Debug
```

## Credentials
CMake & Github Actions are based on https://github.com/TheLartians/ModernCppStarter
