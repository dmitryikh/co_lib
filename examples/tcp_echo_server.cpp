#include <co/co.hpp>
#include <co/net/net.hpp>

co::func<void> serve_client(co::net::tcp_stream tcp)
{
    constexpr size_t buffer_size = 100;
    std::array<char, buffer_size> buffer;
    std::span<char> received_data = co_await tcp.read(buffer).unwrap();
    std::cout << "Request is: " << std::string_view{ received_data.data(), received_data.size() } << "\n";

    std::cout << "Send it back\n";
    co_await tcp.write(received_data).unwrap();
    co_await tcp.shutdown().unwrap();
}

co::func<void> server(std::string ip, uint16_t port)
{
    try
    {
        auto listener = co_await co::net::tcp_listener::bind(ip, port).unwrap();

        co::stop_source stop_source;
        auto signal_scoped = co::signal_ctrl_c(stop_source);

        auto token = stop_source.get_token();

        while (true)
        {
            co::net::tcp_stream tcp_stream = co_await listener.accept(co::until::cancel(token)).unwrap();
            std::cout << tcp_stream.peer_address() << " connected\n";
            co::thread(serve_client(std::move(tcp_stream))).detach();
        }
    }
    catch (const co::exception& coexc)
    {
        std::cerr << "co_lib error: " << coexc << '\n';
    }
    catch (const std::exception& exc)
    {
        std::cerr << "unknown error: " << exc.what() << '\n';
    }
}

int main()
{
    co::loop(server("0.0.0.0", 50007));
}