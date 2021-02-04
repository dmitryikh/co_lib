#include <chrono>
#include <iostream>
#include <variant>
#include <experimental/coroutine>
#include "tmp/generator.hpp"
#include "tmp/eager.hpp"
#include "tmp/lazy.hpp"
#include <co/scheduler.hpp>
#include <co/sleep.hpp>
#include <co/mutex.hpp>
#include <co/net/network.hpp>
#include <co/thread.hpp>
#include <co/co.hpp>
#include <co/channel.hpp>
#include <co/one_shot.hpp>
#include <co/condition_variable.hpp>
#include <co/future.hpp>

using namespace std::chrono_literals;

co::tmp::eager<int> some_value_fast(int x, int y)
{
    std::cout << "about to return from some_value_fast" << std::endl;
    co_return x + y;
}

co::tmp::eager<int> some_value_throw(int x, int y)
{
    std::cout << "about to return from some_value_throw" << std::endl;
    throw std::runtime_error("bla bla bla");
    co_return x + y;
}

// eager<void> some_value_void(int /*x*/, int /*y*/)
// {
//     co_return;
// }

co::tmp::lazy<int> some_value_lazy(int x, int y)
{
    std::cout << "about to return from some_value_lazy" << std::endl;
    co_return x + y;
}

co::tmp::lazy<void> some_lazy_void(int /*x*/, int /*y*/)
{
    co_return;
}

co::tmp::generator<uint64_t> fibonacci()
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
    co::tmp::eager<int> res = some_value_fast(10, 12);
    std::cout << "res value = " << std::endl;
    std::cout << res.value() << std::endl;

    co::tmp::eager<int> res2 = some_value_throw(12, 14);
    std::cout << "res2 value = " << std::endl;
    // std::cout << res2.value() << std::endl;

    // eager<void> res3 = some_value_void(12, 14);
    // std::cout << "res3 value = " << std::endl;
    // res3.value();
}

void lazy_usage()
{
    co::tmp::lazy<int> res = some_value_lazy(10, 12);
    std::cout << "lazy: res value = " << std::endl;
    std::cout << "lazy: " << res.value() << std::endl;
    // res.value();  will throw

    co::tmp::lazy<void> res2 = some_lazy_void(10, 12);
    std::cout << "lazy: res2 value = " << std::endl;
    res2.value();
}

co::func<void> func10()
{
    for (size_t i = 0; i < 1; i++)
    {
        co_await co::this_thread::sleep_for(3s);
        std::cout << "func10 running\n";
    }
    std::cout << "func10 finished\n";
}

co::func<void> func1()
{
    co::thread(func10()).detach();
    for (size_t i = 0; i < 3; i++)
    {
        co_await co::this_thread::sleep_for(1s);
        std::cout << "func1 running\n";
    }
    std::cout << "func1 finished\n";
}

co::func<void> func2_nested()
{
    for (size_t i = 0; i < 10; i++)
    {
        co_await co::this_thread::sleep_for(300ms);
        std::cout << "func2_nested running\n";
    }
    std::cout << "func2_nested finished\n";
    throw std::runtime_error("exceptioN!!");
}

co::func<void> func2()
{
    std::cout << "func2 running\n";
    co_await func2_nested();
}

void scheduler_usage()
{
    co::loop([] () -> co::func<void> 
    {
        co::thread(func2()).detach();
        co::thread(func1()).detach();
        co_return;
    }());
}

co::func<void> client_work(const std::string& ip, uint16_t port)
{
    auto socket = (co_await co::net::connect(ip, port)).unwrap();
    // co_await socket.read_n(&bytes[0], 30);
    auto th = co::thread([] (auto& socket) -> co::func<void>
    {
        for (int i = 0; i < 3; i++)
        {
            const std::string to_write = "abba" + std::to_string(i);
            co_await socket.write(to_write.data(), to_write.size());
            co_await co::this_thread::sleep_for(1000ms);
        }
        std::cout << "shutdown\n";
        co_await socket.shutdown();
    }(socket), "writer");

    while (true)
    {
        std::string bytes(30, 0);
        auto res = co_await socket.read(&bytes[0], 30);
        if (res == co::net::eof)
        {
            break;
        }
        else if (res.is_err())
        {
            std::cerr << res << "\n";
            auto _ = co_await socket.shutdown();
            break;
        }
        bytes.resize(res.unwrap());
        std::cout << "read res: " << bytes << "\n";
    }
    co_await th.join();
}

void net_usage()
{
    co::loop(client_work("0.0.0.0", 50007));
}

void mutex_usage()
{
    co::loop([]() -> co::func<void>
    {
        co::mutex mutex;
        auto th1 = co::thread([](auto& mutex) -> co::func<void>
        {
            std::cout << "func0 about to get lock\n";
            co_await mutex.lock();
            std::cout << "func0 got lock\n";
            co_await co::this_thread::sleep_for(1s);
            std::cout << "func0 release lock\n";
            mutex.unlock();
        }(mutex));
        auto th2 = co::thread([](auto& mutex) -> co::func<void>
        {
            co_await mutex.lock();
            std::cout << "func1 got lock\n";
            co_await co::this_thread::sleep_for(1s);
            std::cout << "func1 release lock\n";
            mutex.unlock();
        }(mutex));
        auto th3 = co::thread([](auto& mutex) -> co::func<void>
        {
            while (co_await mutex.lock_for(200ms) == co::timeout)
            {
                std::cout << "func2 trying to get lock\n";
            }
            std::cout << "func2 got lock\n";
            co_await co::this_thread::sleep_for(1s);
            std::cout << "func2 release lock\n";
            mutex.unlock();

        }(mutex));
        co_await th1.join();
        co_await th2.join();
        co_await th3.join();
        co_return;
    }());
}

void stop_token_usage()
{
    co::loop([]() -> co::func<void>
    {
        auto th = co::thread([]() -> co::func<void>
        {
            while (true)
            {
                std::cout << "thread1: about to sleep\n";
                const auto res = co_await co::this_thread::sleep_for(2s, co::this_thread::get_stop_token());
                if (res == co::cancel)
                    co_return;
            }
        }());

        auto th2 = co::thread([](auto stop) -> co::func<void>
        {
            while (true)
            {
                std::cout << "thread2: about to sleep\n";
                const auto res = co_await co::this_thread::sleep_for(600ms, stop);
                if (res == co::cancel)
                    co_return;
            }
        }(th.get_stop_token()));

        co_await co::this_thread::sleep_for(2500ms);
        th.request_stop();
        std::cout << "stop request sended\n";
        co_await th2.join();
        co_await th.join();
        std::cout << "joined\n";
    }());
}

void dangling_ref()
{
    co::loop([]() -> co::func<void>
    {
        // This is crashed:
        // auto i_ptr = std::make_unique<int>(10);
        // auto func = [i_ptr = std::move(i_ptr)]() -> co::func<void>
        // {
        //     std::cout << "i is " << *i_ptr << std::endl;
        //     co_return;
        // }();
        // co_await func;

        auto i_ptr = std::make_unique<int>(10);
        auto func = [](auto i_ptr) -> co::func<void>
        {
            std::cout << "i is " << *i_ptr << std::endl;
            co_return;
        }(std::move(i_ptr));
        co_await func;
    }());
}

void channel_usage()
{
    co::loop([]() -> co::func<void>
    {
        co::channel<int> ch(3);
        auto th1 = co::thread([](auto& ch) -> co::func<void>
        {
            try
            {
                for (int i = 0; i < 100; i++)
                {
                    (co_await ch.push_for(i, 110ms)).unwrap();
                    std::cout << co::this_thread::get_name() << ": pushed " << i << "\n";
                }
            }
            catch (const co::exception& coexc)
            {
                std::cout << "coexc: " << coexc << "\n";
            }
            ch.close();
        }(ch), "producer");

        auto th2 = co::thread([](auto& ch) -> co::func<void>
        {
            while (true)
            {
                auto val = co_await ch.pop();
                if (val == co::closed)
                    break;
                std::cout << co::this_thread::get_name() << ": poped " << val.unwrap() << "\n";
                co_await co::this_thread::sleep_for(100ms);
            }
        }(ch), "consumer1");

        auto th3 = co::thread([](auto& ch) -> co::func<void>
        {
            while (true)
            {
                auto val = co_await ch.pop();
                if (val == co::closed)
                    break;
                std::cout << co::this_thread::get_name() << ": poped " << val.unwrap() << "\n";
                co_await co::this_thread::sleep_for(100ms);
            }
        }(ch), "consumer2");

        co_await th1.join();
        co_await th2.join();
        co_await th3.join();
    }());
}

void one_shot_usage()
{
    co::loop([]() -> co::func<void>
    {
        co::one_shot<std::string> ch;
        auto th1 = co::thread([](auto& ch) -> co::func<void>
        {
            try
            {
                co_await co::this_thread::sleep_for(50ms);
                ch.push("hello world!").unwrap();
                co_await co::this_thread::sleep_for(50ms);
                ch.push("hello world2!").unwrap(); // should throw
            }
            catch (const co::exception& coexc)
            {
                std::cout << "coexc: " << coexc << "\n";
            }
            ch.close();
        }(ch), "producer");

        auto th2 = co::thread([](auto& ch) -> co::func<void>
        {
            while (true)
            {
                auto val = co_await ch.pop_for(10ms);
                if (val == co::closed)
                    break;
                if (val == co::timeout)
                {
                    std::cout << co::this_thread::get_name() << ": timeouted\n";
                    continue;
                }
                std::cout << co::this_thread::get_name() << ": poped " << val.unwrap() << "\n";
                break;
            }
        }(ch), "consumer");

        co_await th1.join();
        co_await th2.join();
    }());
}
void cond_var_usage()
{
    co::loop([]() -> co::func<void>
    {
        co::condition_variable cv;
        std::string data;
        bool ready = false;

        auto th1 = co::thread([](auto& cv, auto& data, auto& ready) -> co::func<void>
        {
            co_await co::this_thread::sleep_for(100ms);
            data = "hello world";
            ready = true;
            cv.notify_one();
        }(cv, data, ready));

        auto th2 = co::thread([](auto& cv, auto& data, auto& ready) -> co::func<void>
        {
            try
            {
                (co_await cv.wait_for(200ms, [&]() { return ready; })).unwrap();
                std::cout << co::this_thread::get_name() << ": data is ready: " << data << "\n";
                ready = false;
            }
            catch (const co::exception& coexc)
            {
                std::cout << co::this_thread::get_name() << ": coexc: " << coexc << "\n";
            }
        }(cv, data, ready), "consumer1");

        auto th3 = co::thread([](auto& cv, auto& data, auto& ready) -> co::func<void>
        {
            try
            {
                (co_await cv.wait_for(200ms, [&]() { return ready; })).unwrap();
                std::cout << co::this_thread::get_name() << ": data is ready: " << data << "\n";
                ready = false;
            }
            catch (const co::exception& coexc)
            {
                std::cout << co::this_thread::get_name() << ": coexc: " << coexc << "\n";
            }
        }(cv, data, ready), "consumer2");

        co_await th1.join();
        co_await th2.join();
        co_await th3.join();
    }());
}

void future_usage()
{
    co::loop([]() -> co::func<void>
    {
        co::promise<std::string> promise;
        auto future = promise.get_future();

        co::thread([](auto promise) -> co::func<void>
        {
            co_await co::this_thread::sleep_for(1000ms);
            promise.set_value("hello world");
        }(std::move(promise))).detach();

        std::cout << co_await future.get() << std::endl;
    }());
}

int main()
{
    // usage();
    // eager_usage();
    // lazy_usage();
    // scheduler_usage();
    // net_usage();
    // mutex_usage();
    // stop_token_usage();
    // dangling_ref();
    // channel_usage();
    // one_shot_usage();
    // cond_var_usage();
    future_usage();
}