#include <chrono>
#include <iostream>
#include <co/co.hpp>

using namespace std::chrono_literals;

int main()
{
    co::loop(
        []() -> co::func<void>
        {
            co::promise<std::string> promise;
            auto future = promise.get_future();

            co::thread(
                [](auto promise) -> co::func<void>
                {
                    co_await co::this_thread::sleep_for(100ms);
                    promise.set_value("hello world");
                }(std::move(promise)))
                .detach();

            std::cout << co_await future.get() << std::endl;
        }());
}