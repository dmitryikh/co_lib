#pragma once

#include <cassert>
#include <memory>
#include <uv.h>

namespace co::impl
{

struct uv_handle_deleter
{
    template <typename T>
    void operator()(T* handle)
    {
        static auto on_close = [](uv_handle_t* handle)
        {
            assert(handle != nullptr);
            delete handle;
        };
        uv_close((uv_handle_t*)handle, on_close);
    }
};

template <typename T>
using uv_handle_ptr = std::unique_ptr<T, uv_handle_deleter>;

}  // namespace co::impl