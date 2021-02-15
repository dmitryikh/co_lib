#include <co/co.hpp>
#include <co/net/net.hpp>

co::func<void> client(std::string ip, uint16_t port)
{
    try
    {
        auto tcp_stream = co_await co::net::tcp_stream::connect(ip, port).unwrap();

        std::string send_data = "hello world";
        std::cout << "Send message: " << send_data << "\n";
        co_await tcp_stream.write(send_data.data(), send_data.size()).unwrap();

        const size_t buffer_size = 100;
        std::string receive_data(buffer_size, '\0');
        size_t read_len = co_await tcp_stream.read(receive_data.data(), receive_data.size()).unwrap();
        receive_data.resize(read_len);
        std::cout << "Reply is: " << receive_data << "\n";
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