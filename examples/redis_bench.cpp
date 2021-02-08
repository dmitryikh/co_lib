#include <co/redis/connection.hpp>
#include <co/redis/client.hpp>
#include <co/co.hpp>

using namespace std::chrono_literals;

int main()
{
    co::loop([]() -> co::func<void>
    {
        for (int i = 0; i < 1; i++)
        {
            co::thread([]() -> co::func<void>
            {
                auto client = co::redis::client("127.0.0.1", 6379);
                co_await client.wait_until_connected();

                size_t ok = 0;
                size_t nok = 0;

                const auto start = std::chrono::steady_clock::now();

                std::vector<co::future<co::result<co::redis::reply>>> replies;
                replies.reserve(1000000 * 2);

                for (int i = 0; i < 1000000; i++)
                {
                    auto f1 = co_await client.set(std::to_string(i), "sun" + std::to_string(i));
                    // auto f2 = co_await client.get(std::to_string(i));
                    replies.push_back(std::move(f1));
                    // replies.push_back(std::move(f2));
                }
                std::cout << "do flush\n";
                co_await client.flush();

                const auto until = std::chrono::steady_clock::now() + 1s;
                std::cout << "wait replies\n";
                for (auto& f : replies)
                {
                    auto r = co_await f.get_until(until);

                    if (r.is_err())
                    {
                        nok++;
                    }
                    else
                        ok++;
                }

                const auto end = std::chrono::steady_clock::now();

                int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

                std::cout << "FINISH in " << ms << "ms. ok = " << ok <<", nok = " << nok << "\n";

                client.close();
                co_await client.join();
            }()).detach();
        }
        
        co_return;
    }());
}