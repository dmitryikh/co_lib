#pragma once
#include <uv.h>
#include <co/func.hpp>

namespace co::impl
{

// Wakes a suspended co::thread. Can be used from another std::thread.
class async_signal
{
public:
    typedef void(*callback_type)(void*);
    // Can't be moved or copied because uv's loop keep the reference to uv_async.
    async_signal(const async_signal&) = delete;
    async_signal(async_signal&&) = delete;
    async_signal& operator=(const async_signal&) = delete;
    async_signal& operator=(async_signal&&) = delete;
    async_signal() = default;

    void init(uv_loop_t* uv_loop)
    {
        // TODO: check the return value
        uv_async_init(uv_loop, &uv_async, &async_callback);
        uv_async.data = static_cast<void*>(this);
    }

    void send()
    {
        uv_async_send(&uv_async);
    }
    co::func<void> close();

    // This will be called inside the co::thread's std::thread.
    static void async_callback(uv_async_t* handle)
    {
        assert(handle->data != nullptr);
        async_signal& self = *static_cast<async_signal*>(handle->data);
        assert(self.data != nullptr);
        assert(self.callback != nullptr);
        self.callback(self.data);
    }

    uv_async_t uv_async;
    void* data = nullptr;
    callback_type callback = nullptr;
};
}  // co::impl