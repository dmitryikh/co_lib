#include <co/co.hpp>
#include <co/net/net.hpp>

co::func<void> client(std::string ip, uint16_t port)
{
    try
    {
        auto tcp_stream = co_await co::net::tcp_stream::connect(ip, port).unwrap();

        std::string send_data = "hello world";
        std::cout << "Send message: " << send_data << "\n";
        co_await tcp_stream.write(send_data).unwrap();

        const size_t buffer_size = 100;
        std::array<char, buffer_size> buffer;
        std::span<char> received_data = co_await tcp_stream.read(buffer).unwrap();
        std::cout << "Reply is: " << std::string_view{ received_data.data(), received_data.size() } << "\n";

        co_await tcp_stream.shutdown().unwrap();
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
    co::loop(client("0.0.0.0", 50007));
}
