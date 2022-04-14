#include <catch2/catch.hpp>
#include <thread>
#include <co/co.hpp>

using namespace std::chrono_literals;

TEST_CASE("event usage", "[core]")
{
    std::vector<co::event> events(1000);
    co::loop(
        [&events]() -> co::func<void>
        {
            for (auto& event : events)
            {
                co::thread(
                    [&event]() -> co::func<void>
                    {
                        co_await co::this_thread::sleep_for(2ms);
                        event.notify();
                        co_return;
                    });
            }
            for (auto& event : events)
            {
                co::thread([&event]() -> co::func<void> { co_await event.wait(); });
            }
            co_return;
        });

    for (auto& event : events)
    {
        REQUIRE(event.is_notified());
    }
}
TEST_CASE("ts::event blocking usage", "[core][ts]")
{
    constexpr int n_events = 1000;
    constexpr int n_threads = 10;
    std::vector<co::ts::event> events(n_events);
    int events_counter = 0;
    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (int i = 0; i < n_threads; i++)
    {
        auto th = std::thread([&events]()
            {
                co::loop(
                    [&events]() -> co::func<void>
                    {
                        for (auto& event : events)
                        {
                            co::thread(
                                [&event]() -> co::func<void>
                                {
                                    co_await co::this_thread::sleep_for(2ms);
                                    event.notify();
                                    co_return;
                                });
                        }
                        co_return;
                    }
                );
            });
        threads.push_back(std::move(th));
    }
    for (auto& event : events)
    {
        event.blocking_wait();
        events_counter++;
    }

    REQUIRE(events_counter == n_events);
    for (auto& event : events)
    {
        REQUIRE(event.is_notified());
    }
    for (auto& thread : threads)
    {
        thread.join();
    }
}
TEST_CASE("ts::event usage", "[core][ts]")
{
    constexpr int n_events = 1000;
    constexpr int n_threads = 10;
    std::vector<co::ts::event> events(n_events);
    int events_counter = 0;
    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (int i = 0; i < n_threads; i++)
    {
        auto th = std::thread([&events]()
            {
                co::loop(
                    [&events]() -> co::func<void>
                    {
                        for (auto& event : events)
                        {
                            co::thread(
                                [&event]() -> co::func<void>
                                {
                                    co_await co::this_thread::sleep_for(2ms);
                                    event.notify();
                                });
                        }
                        co_return;
                    }
                );
            });
        threads.push_back(std::move(th));
    }
    co::loop(
        [&events, &events_counter]() -> co::func<void>
        {
            for (auto& event : events)
            {
                co::thread([&event, &events_counter]() -> co::func<void>
                {
                    co_await event.wait();
                    events_counter++;
                });
            }
            co_return;
        });
    REQUIRE(events_counter == n_events);
    for (auto& event : events)
    {
        REQUIRE(event.is_notified());
    }
    for (auto& thread : threads)
    {
        thread.join();
    }
}
TEMPLATE_TEST_CASE("event wait/notify", "[core]", co::event, co::ts::event)
{
    co::loop(
        []() -> co::func<void>
        {
            TestType event;
            REQUIRE(event.is_notified() == false);

            REQUIRE(event.notify() == true);
            REQUIRE(event.is_notified() == true);

            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);

            co_await event.wait();
            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);

            co_await event.wait();
            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);
            co_return;
        });

    co::loop(
        []() -> co::func<void>
        {
            TestType event;
            bool notified = false;
            auto th = co::thread(
                [&event, &notified]() -> co::func<void>
                {
                    REQUIRE(event.is_notified() == false);

                    REQUIRE(event.notify() == true);
                    REQUIRE(event.is_notified() == true);
                    notified = true;

                    REQUIRE(event.notify() == false);
                    REQUIRE(event.is_notified() == true);
                    co_return;
                });

            co_await event.wait();
            REQUIRE(notified == true);
            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);

            co_await event.wait();
            REQUIRE(event.notify() == false);
            REQUIRE(event.is_notified() == true);
            co_await th.join();
        });
}

TEST_CASE("event notify in advance", "[core]")
{
    co::loop(
        []() -> co::func<void>
        {
          co::event event;
          REQUIRE(event.is_notified() == false);

          REQUIRE(event.notify() == true);
          REQUIRE(event.is_notified() == true);

          REQUIRE(event.notify() == false);
          REQUIRE(event.is_notified() == true);

          co_await event.wait();
          REQUIRE(event.notify() == false);
          REQUIRE(event.is_notified() == true);

          co_await event.wait();
          REQUIRE(event.notify() == false);
          REQUIRE(event.is_notified() == true);

          // all interruptable cases will return (result::is_ok() == true) on already notified event
          auto res = co_await event.wait(100ms);
          REQUIRE(res.is_ok());

          res = co_await event.wait(std::chrono::steady_clock::now() + 100ms);
          REQUIRE(res.is_ok());

          auto stop_source = co::stop_source();
          const auto stop_token = stop_source.get_token();
          res = co_await event.wait(co::until(100ms, stop_token));
          REQUIRE(res.is_ok());

          stop_source.request_stop();
          res = co_await event.wait(co::until(100ms, stop_token));
          REQUIRE(res.is_ok());
        });
}

TEST_CASE("event never notified", "[core]")
{
    co::loop(
        []() -> co::func<void>
        {
          co::event event;

          REQUIRE(event.is_notified() == false);

          // this will lock infinitely
          // co_await event.wait();

          {
              auto start = std::chrono::steady_clock::now();
              auto res = co_await event.wait(100ms);
              REQUIRE(res == co::timeout);
              REQUIRE(event.is_notified() == false);
              // TODO: fix rounding problem
              REQUIRE(std::chrono::steady_clock::now() - start >= 100ms - 1ms);
          }

          {
              auto deadline = std::chrono::steady_clock::now() + 100ms;
              auto res = co_await event.wait(deadline);
              REQUIRE(res == co::timeout);
              REQUIRE(event.is_notified() == false);
              REQUIRE(std::chrono::steady_clock::now() >= deadline - 1ms);
          }

          auto stop_source = co::stop_source();
          const auto stop_token = stop_source.get_token();
          // stop is already requested
          stop_source.request_stop();

          {
              auto start = std::chrono::steady_clock::now();
              auto res = co_await event.wait(stop_token);
              REQUIRE(res == co::cancel);
              REQUIRE(event.is_notified() == false);
              REQUIRE(std::chrono::steady_clock::now() - start < 1ms);
          }

          {
              auto start = std::chrono::steady_clock::now();
              auto res = co_await event.wait(co::until(100ms, stop_token));
              REQUIRE(res == co::cancel);
              REQUIRE(event.is_notified() == false);
              REQUIRE(std::chrono::steady_clock::now() - start < 1ms);
          }

          {
              auto stop_source = co::stop_source();
              const auto stop_token = stop_source.get_token();
              // stop will be requested after 50ms
              auto th = co::thread(
                  [&stop_source]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(50ms);
                    stop_source.request_stop();
                  });
              auto start = std::chrono::steady_clock::now();
              auto res = co_await event.wait(co::until(100ms, stop_token));
              REQUIRE(res == co::cancel);
              REQUIRE(event.is_notified() == false);
              REQUIRE(std::chrono::steady_clock::now() - start >= 50ms - 1ms);
              REQUIRE(std::chrono::steady_clock::now() - start < 100ms);
              co_await th.join();
          }
        });
}

TEST_CASE("event notified concurrently", "[core]")
{
    co::loop(
        []() -> co::func<void>
        {
          {
              co::event event;

              auto th = co::thread(
                  [&event]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(50ms);
                    event.notify();
                  });

              auto start = std::chrono::steady_clock::now();
              co_await event.wait();
              REQUIRE(std::chrono::steady_clock::now() - start >= 50ms - 1ms);
              co_await th.join();
          }
          {
              co::event event;

              auto th = co::thread(
                  [&event]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(50ms);
                    event.notify();
                  });

              auto start = std::chrono::steady_clock::now();
              auto res = co_await event.wait(25ms);
              REQUIRE(res == co::timeout);
              REQUIRE(std::chrono::steady_clock::now() - start >= 25ms - 1ms);
              REQUIRE(std::chrono::steady_clock::now() - start < 50ms);
              co_await th.join();
          }
          {
              co::event event;

              auto th = co::thread(
                  [&event]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(50ms);
                    event.notify();
                  });

              auto start = std::chrono::steady_clock::now();
              auto res = co_await event.wait(100ms);
              REQUIRE(res.is_ok());
              REQUIRE(std::chrono::steady_clock::now() - start >= 50ms - 1ms);
              co_await th.join();
          }
          {
              co::event event;
              auto stop_source = co::stop_source();
              auto stop_token = stop_source.get_token();

              auto th1 = co::thread(
                  [&event]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(50ms);
                    event.notify();
                  });
              auto th2 = co::thread(
                  [&stop_source]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(25ms);
                    stop_source.request_stop();
                  });

              auto start = std::chrono::steady_clock::now();
              auto res = co_await event.wait(co::until(100ms, stop_token));
              REQUIRE(res == co::cancel);
              REQUIRE(std::chrono::steady_clock::now() - start >= 25ms - 1ms);
              REQUIRE(std::chrono::steady_clock::now() - start < 50ms);
              co_await th1.join();
              co_await th2.join();
          }
          {
              co::event event;
              auto stop_source = co::stop_source();
              auto stop_token = stop_source.get_token();

              auto th1 = co::thread(
                  [&event]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(25ms);
                    event.notify();
                  });
              auto th2 = co::thread(
                  [&stop_source]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(50ms);
                    stop_source.request_stop();
                  });

              auto start = std::chrono::steady_clock::now();
              auto res = co_await event.wait(co::until(100ms, stop_token));
              REQUIRE(res.is_ok());
              REQUIRE(std::chrono::steady_clock::now() - start >= 25ms - 1ms);
              REQUIRE(std::chrono::steady_clock::now() - start < 50ms);
              co_await th1.join();
              co_await th2.join();
          }
          {
              co::event event;
              auto stop_source = co::stop_source();
              auto stop_token = stop_source.get_token();

              auto th1 = co::thread(
                  [&event]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(50ms);
                    event.notify();
                  });
              auto th2 = co::thread(
                  [&stop_source]() -> co::func<void>
                  {
                    co_await co::this_thread::sleep_for(50ms);
                    stop_source.request_stop();
                  });

              auto start = std::chrono::steady_clock::now();
              auto res = co_await event.wait(co::until(25ms, stop_token));
              REQUIRE(res == co::timeout);
              REQUIRE(std::chrono::steady_clock::now() - start >= 25ms - 1ms);
              REQUIRE(std::chrono::steady_clock::now() - start < 50ms);
              co_await th1.join();
              co_await th2.join();
          }
        });
}
