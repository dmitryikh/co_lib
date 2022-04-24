#include <catch2/catch.hpp>
#include <co/channel.hpp>
#include <co/co.hpp>
#include <co/ts_channel.hpp>

#include <thread>

using namespace std::chrono_literals;

TEMPLATE_TEST_CASE("channel usage", "[primitives]", co::channel<int>, co::ts_channel<int>)
{
    auto start = std::chrono::steady_clock::now();
    co::loop(
        []() -> co::func<void>
        {
            TestType ch(3);
            auto th1 = co::thread(
                [ch]() mutable -> co::func<void>
                {
                    for (int i = 0; i < 10; i++)
                    {
                        co_await ch.push(i).unwrap();
                    }
                    ch.close();
                },
                "producer");

            auto th2 = co::thread(
                [ch]() mutable -> co::func<void>
                {
                    while (true)
                    {
                        auto val = co_await ch.pop();
                        if (val == co::closed)
                            break;
                        co_await co::this_thread::sleep_for(10ms);
                    }
                },
                "consumer1");

            auto th3 = co::thread(
                [ch]() mutable -> co::func<void>
                {
                    while (true)
                    {
                        auto val = co_await ch.pop();
                        if (val == co::closed)
                            break;
                        co_await co::this_thread::sleep_for(5ms);
                    }
                },
                "consumer2");

            co_await th1.join();
            co_await th2.join();
            co_await th3.join();
        }());
    auto end = std::chrono::steady_clock::now();
    REQUIRE(end - start > 20ms);
    // TODO: sometimes it runs out of time..
    REQUIRE(end - start < 50ms + 5ms);
}

TEST_CASE("channel simple usage", "[primitives]")
{
    co::loop(
        []() -> co::func<void>
        {
            const size_t capacity = 2;
            co::channel<int> ch(capacity);

            auto th1 = co::thread(
                [ch]() mutable -> co::func<void>
                {
                    co_await ch.push(1).unwrap();
                    co_await ch.push(2).unwrap();
                    co_await ch.push(3).unwrap();
                    ch.close();
                },
                "producer");

            while (true)
            {
                auto val = co_await ch.pop();
                if (val == co::closed)
                    break;
            }
            co_await th1.join();
        }());
}

TEST_CASE("ts::channel basic stress test", "[ts][primitives][stress]")
{
    // Use std::thread and co::thread for push/pop simultaneously.
    // Do not use interruptions (timeout, stop_tokens, etc).
    constexpr int n_elements = 10000;
    constexpr int n_capacity = 100;
    constexpr int n_threads = 10;
    co::ts_channel<std::string> ch(n_capacity);
    int events_counter = 0;
    std::vector<std::thread> sender_threads, receiver_threads;
    sender_threads.reserve(n_threads * 2);
    receiver_threads.reserve(n_threads * 2);
    std::atomic<int> reciever_counter = 0;

    // receivers threads for blocking_pop
    for (int i = 0; i < n_threads; i++)
    {
        auto th = std::thread(
            [ch, &reciever_counter]() mutable
            {
                while (true)
                {
                    co::result<std::string> result = ch.blocking_pop();
                    if (result == co::closed)
                        break;
                    std::string local_copy = result.unwrap();
                    assert(local_copy.size() > 0);
                    reciever_counter.fetch_add(1, std::memory_order::release);
                }
            });
        receiver_threads.push_back(std::move(th));
    }
    // receivers threads for non-blocking pop
    for (int i = 0; i < n_threads; i++)
    {
        auto th = std::thread(
            [ch, &reciever_counter]() mutable
            {
                co::loop(
                    [&ch, &reciever_counter]() -> co::func<void>
                    {
                        while (true)
                        {
                            co::result<std::string> result = co_await ch.pop();
                            if (result == co::closed)
                                break;
                            std::string local_copy = result.unwrap();
                            assert(local_copy.size() > 0);
                            reciever_counter.fetch_add(1, std::memory_order::release);
                        }
                    });
            });
        receiver_threads.push_back(std::move(th));
    }

    // Senders are in the blocking threads.
    for (int i = 0; i < n_threads; i++)
    {
        auto th = std::thread(
            [ch, n_threads, n_elements]() mutable
            {
                for (int i = 0; i < n_elements / (2 * n_threads); i++)
                {
                    co::result<void> result = ch.blocking_push(std::to_string(i));
                    assert(result.is_ok());
                }
            });
        sender_threads.push_back(std::move(th));
    }
    // Senders are in the non-blocking threads.
    for (int i = 0; i < n_threads; i++)
    {
        auto th = std::thread(
            [ch, n_threads, n_elements]() mutable
            {
                co::loop(
                    [&ch]() -> co::func<void>
                    {
                        for (int i = 0; i < n_elements / (2 * n_threads); i++)
                        {
                            co::result<void> result = co_await ch.push(std::to_string(i));
                            assert(result.is_ok());
                        }
                    });
            });
        sender_threads.push_back(std::move(th));
    }
    for (auto& thread : sender_threads)
    {
        thread.join();
    }
    ch.close();
    for (auto& thread : receiver_threads)
    {
        thread.join();
    }
    REQUIRE(reciever_counter.load(std::memory_order::acquire) == n_elements);
}