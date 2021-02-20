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
