#include <catch2/catch.hpp>
#include <co/co.hpp>
#include <co/net/tcp_listener.hpp>
#include <co/net/tcp_stream.hpp>

using namespace std::chrono_literals;

size_t n_requests = 0;
const size_t n_clients = 100;

co::func<void> serve_client(co::net::tcp_stream tcp)
{
    constexpr size_t buffer_size = 100;
    std::array<char, buffer_size> buffer;
    std::span<char> received_data = co_await tcp.read(buffer).unwrap();

    co_await tcp.write(received_data).unwrap();
    co_await tcp.shutdown().unwrap();
    n_requests++;
}

co::func<void> server(std::string ip, uint16_t port)
{
    try
    {
        auto token = co::this_thread::stop_token();
        auto listener = co_await co::net::tcp_listener::bind(ip, port).unwrap();

        while (true)
        {
            auto res = co_await listener.accept(co::until::cancel(token));
            if (res == co::cancel)
                break;
            REQUIRE(res.unwrap().peer_address().ip() == "127.0.0.1");
            REQUIRE(res.unwrap().peer_address().family() == co::net::ip4);
            REQUIRE(res.unwrap().local_address().ip() == "127.0.0.1");
            REQUIRE(res.unwrap().local_address().port() == port);
            REQUIRE(res.unwrap().local_address().family() == co::net::ip4);
            co::thread(serve_client(std::move(res.unwrap()))).detach();
        }
    }
    catch (const std::exception& exc)
    {
        std::cerr << "error: " << exc.what() << '\n';
        REQUIRE(false);
    }
}

co::func<void> client(std::string ip, uint16_t port)
{
    try
    {
        auto tcp_stream = co_await co::net::tcp_stream::connect(ip, port).unwrap();

        REQUIRE(tcp_stream.peer_address().ip() == "127.0.0.1");
        REQUIRE(tcp_stream.peer_address().port() == port);
        REQUIRE(tcp_stream.peer_address().family() == co::net::ip4);
        REQUIRE(tcp_stream.local_address().ip() == "127.0.0.1");
        REQUIRE(tcp_stream.local_address().family() == co::net::ip4);

        std::string send_data = "hello world";
        co_await tcp_stream.write(send_data).unwrap();

        const size_t buffer_size = 100;
        std::array<char, buffer_size> buffer;
        std::span<char> received_data = co_await tcp_stream.read(buffer).unwrap();

        co_await tcp_stream.shutdown().unwrap();
    }
    catch (const std::exception& exc)
    {
        std::cerr << "error: " << exc.what() << '\n';
        REQUIRE(false);
    }
}

TEST_CASE("tcp client/server", "[net]")
{
    co::loop(
        []() -> co::func<void>
        {
            auto server_th = co::thread([]() -> co::func<void> { co_await server("0.0.0.0", 50007); });
            co_await co::this_thread::sleep_for(10ms);

            std::vector<co::thread> clients;
            for (size_t i = 0; i < n_clients; i++)
            {
                auto th = co::thread(client("0.0.0.0", 50007));
                clients.push_back(std::move(th));
            }

            for (auto& client : clients)
            {
                co_await client.join();
            }

            server_th.request_stop();
            co_await server_th.join();
        });
    REQUIRE(n_requests == n_clients);
}
