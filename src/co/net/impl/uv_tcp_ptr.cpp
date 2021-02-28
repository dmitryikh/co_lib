#include <co/net/impl/uv_tcp_ptr.hpp>

#include <co/impl/scheduler.hpp>

namespace co::net::impl
{

uv_tcp_ptr make_and_init_uv_tcp_handle()
{
    auto handle = new uv_tcp_t;
    uv_tcp_init(co::impl::get_scheduler().uv_loop(), handle);
    return uv_tcp_ptr{ handle };
}

}  // namespace co::net::impl
