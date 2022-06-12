#include <iostream>
#include <chrono>

// "co/co.hpp" contains everything that you need to work with co_lib
#include <co/co.hpp>

using namespace std::chrono_literals;

// Coroutine is represented in co::func<T> class. co::func<T> is like a
// function, but can be suspended and resumed later in `co_await` points.
co::func<void> do_work()
{
    co_await co::this_thread::sleep_for(1s);
}

int main()
{
    // in order to execute the body of the co::func you can schedule it and run
    // the event loop. Event loop will use the current system thread to execute
    // coroutines until all coroutines will be finished
    co::loop(do_work());

    // Run coroutine sequentially. Note the lambda syntax to be used as unnamed co::func
    co::loop(
        []() -> co::func<void>
        {
            co_await do_work();
            co_await do_work();
        });
    // Total execution time: 2s

    // To Run coroutine asynchronously. You need to run it inside co::thread.
    // every co::thread schedule itself in the event loop independently
    co::loop(
        []() -> co::func<void>
        {
            auto th1 = co::thread(do_work());
            auto th2 = co::thread(do_work());
            // the co::thread execution is already started(scheduled) at this point

            co_await th1.join();
            co_await th2.join();
        });
    // Total execution time: 1s

    // co::result is used to represent co::func result that can be return value
    // or error object describing the error
    co::result<int> res = co::ok(3);  // put success value
    CO_CHECK(res.is_ok());
    int i = res.unwrap();  // get the value or throw co::exception in case of holding an error

    res = co::err(co::timeout);  // std::error_code is used as error in co::result
    CO_CHECK(res.is_err());
    CO_CHECK(res.err() == co::timeout);
    CO_CHECK(res == co::timeout);  // sugar on (res.is_err() && res.err == co::timeout)
    res = co::err(co::cancel,
                  "func is cancelled");  // additional description as a not owned char* can be provided

    // Cancellation is a first class citizen in co_lib. Every co::thread has
    // associated co::stop_source & co::stop_token
    co::loop(
        []() -> co::func<void>
        {
            std::vector<std::string> args{ "aa", "bb", "cc" };
            // co::thread will recognize that you provide a lambda which produces
            // co::func. Lambda object will be automatically wrapped with
            // co::func to be sure that the lambda capture list lives until
            // co::func is done. See co::invoke for details.
            auto th = co::thread(
                [args]() -> co::func<void>
                {
                    const co::stop_token token = co::this_thread::stop_token();

                    for (const auto& arg : args)
                    {
                        co::result<void> res = co_await co::this_thread::sleep_for(1s, token);
                        if (res == co::cancel)
                        {
                            std::cout << "cancelled\n";
                            co_return;
                        }
                        std::cout << "done work with arg = " << arg << "\n";
                    }
                    std::cout << "done all args\n";
                });

            co_await co::this_thread::sleep_for(2500ms);
            th.request_stop();
            co_await th.join();
        });
    // Total execution time: 2.5s

    // co_lib provides many synchronization primitives: co::mutex,
    // co::condition_variable, co::future, co::promise, co::channel. They can be
    // used to provide non blocking synchronization between co::threads. Below
    // is a toy example where 3 co::threads are spawned to consume the
    // co::channel, but processing is done under co::mutex. That leads to
    // sequential processing. Thus, it's no sense to use 3 co::threads, but..
    // why not :)
    co::loop(
        []() -> co::func<void>
        {
            const size_t capacity = 3;
            co::channel<std::string> ch(capacity);
            co::mutex mutex;
            std::vector<co::thread> threads;

            for (size_t i = 0; i < 3; i++)
            {
                // co::channel has value semantic and can be cheaply copied.
                // co::mutex can't be copied. We need mutable lambda to be able to
                // change captured values
                auto th = co::thread(
                    [ch, &mutex]() mutable -> co::func<void>
                    {
                        while (true)
                        {
                            co::result<std::string> res = co_await ch.pop(co::until::timeout(1s));
                            if (res == co::timeout)
                                continue;
                            else if (res == co::closed)
                                break;

                            co_await mutex.lock();
                            std::cout << co::this_thread::name() << ": processing " << res.unwrap() << "\n";
                            co_await do_work();
                            mutex.unlock();
                        }
                        std::cout << co::this_thread::name() << ": done\n";
                    });
                threads.push_back(std::move(th));
            }

            for (size_t i = 0; i < 10; i++)
            {
                // ch.push() returns co::func<co::result<void>>. You can use
                // co::func::unwrap() to get new co::func<void> which will throw
                // co::exception in case of co::result::is_err()
                co_await ch.push("package " + std::to_string(i + 1)).unwrap();
            }
            ch.close();

            for (auto& th : threads)
                co_await th.join();
        });
    // Total execution time: 10s
    // Without mutex: 4s
}
