#include <co/co.hpp>
#include <co/net/net.hpp>

co::func<void> serve_client(co::net::tcp_stream tcp)
{
    const size_t buffer_size = 100;
    std::string receive_data(buffer_size, '\0');
    size_t read_len = co_await tcp.read(receive_data.data(), receive_data.size()).unwrap();
    receive_data.resize(read_len);
    std::cout << "Request is: " << receive_data << "\n";

    std::cout << "Send message: " << receive_data << "\n";
    co_await tcp.write(receive_data.data(), receive_data.size()).unwrap();
    co_await tcp.shutdown().unwrap();
}

co::func<void> server(std::string ip, uint16_t port)
{
    try
    {
        auto listener = co_await co::net::tcp_listener::bind(ip, port).unwrap();

        while (true)
        {
            co::net::tcp_stream tcp_stream = co_await listener.accept(co::until{}).unwrap();
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