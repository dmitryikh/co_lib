#include <iostream>
#include <thread>
#include <vector>

#include <co/channel.hpp>
#include <co/co.hpp>

int fib(int n)
{
    if (n <= 1)
        return n;
    return fib(n - 1) + fib(n - 2);
}

int main()
{
    co::ts_channel<int> task_channel(100);
    co::ts_channel<int> result_channel(10);
    constexpr int n_workers = 4;
    constexpr int n_tasks = 1000;
    std::vector<std::thread> workers;
    workers.reserve(n_workers);
    for (int i = 0; i < n_workers; i++)
    {
        auto th = std::thread(
            [task_channel, result_channel]() mutable
            {
                while (true)
                {
                    co::result<int> task = task_channel.blocking_pop();
                    if (task == co::closed)
                        break;
                    const int result = fib(task.unwrap());
                    result_channel.blocking_push(result).unwrap();
                }
            });
        workers.push_back(std::move(th));
    }

    int results_read = 0;
    co::loop(
        [&]() -> co::func<void>
        {
            co::thread(
                [task_channel, n_tasks]() mutable -> co::func<void>
                {
                    for (int i = 0; i < n_tasks; i++)
                    {
                        co_await task_channel.push(25 + (i % 10)).unwrap();
                    }
                    task_channel.close();
                })
                .detach();

            co::thread(
                [result_channel, &results_read]() mutable -> co::func<void>
                {
                    while (true)
                    {
                        co::result<int> result = co_await result_channel.pop();
                        if (result == co::closed)
                            break;
                        results_read++;
                        std::cout << results_read << " result is " << result.unwrap() << "\n";
                        if (results_read == n_tasks)
                            break;
                    }
                })
                .detach();
            co_return;
        });
    for (auto& worker : workers)
    {
        worker.join();
    }
    std::cout << "Done.\n";
    return 0;
}