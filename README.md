![GitHub tag (latest SemVer)](https://img.shields.io/github/v/tag/dmitryikh/co_lib?label=%20version&sort=semver&style=plastic)
[![Ubuntu](https://github.com/dmitryikh/co_lib/actions/workflows/ubuntu.yml/badge.svg?branch=master)](https://github.com/dmitryikh/co_lib/actions/workflows/ubuntu.yml)


# co_lib

co_lib is an experimental asynchronous C++20 framework that feels like std library.

co_lib design priorities:
1. Reuse common patterns and abstractions
2. Make it hard to misuse
3. Performance also matters

co_lib benefits:
1. Cancellation as a first class citizen: co::stop_token, co::stop_source, almost all awaited ops can be cancelled.
2. co::result<T> type (like in Rust) to check the result of an operation
3. Based on libuv C library: event loop, network, timers, etc..
4. Supports "share nothing" patterns. co::ts_channel is the only primitive to talk between OS threads.

Current limitations:
1. Single threaded event loops. Use co::ts_channel to talk between OS threads and co::threads.

# Dependencies
1. C++20 at clang >=11
2. boost 1.75 (headers only)
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
                        std::cout << co::this_thread::name() << ": stop sending - " << coexc << "\n";
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
`co_lib` uses conan package manager to manage its dependencies and also to be included
into other projects.

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
cmake -DCMAKE_BUILD_TYPE=Debug  ..
cmake --build . -j 4
ctest
```

# Documentation
Doxygen based documentations can be generated with the next commands:

```bash
cmake -Hdocs -Bbuild/doc
cmake --build build/doc --target GenerateDocs
open build/doc/doxygen/html/index.html
```

## Thanks To
CMake & Github Actions are based on https://github.com/TheLartians/ModernCppStarter
