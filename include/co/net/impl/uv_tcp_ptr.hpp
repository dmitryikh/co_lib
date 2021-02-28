#pragma once
#include <co/impl/uv_handler.hpp>

namespace co::net::impl
{

using uv_tcp_ptr = co::impl::uv_handle_ptr<uv_tcp_t>;

uv_tcp_ptr make_and_init_uv_tcp_handle();

}