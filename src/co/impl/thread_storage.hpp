#pragma once

#include <string>
#include <co/impl/timer.hpp>
#include <co/stop_token.hpp>

namespace co::impl
{

struct thread_storage
{
    std::string name;
    uint64_t id;
    stop_source stop;
    timer _timer{};
};

inline std::shared_ptr<impl::thread_storage> create_thread_storage(const std::string& thread_name, uint64_t id)
{
    auto storage = std::make_shared<impl::thread_storage>();
    storage->id = id;
    if (thread_name.empty())
        storage->name = "co::thread" + std::to_string(storage->id);
    else
        storage->name = thread_name;

    return storage;
}

inline thread_storage** this_thread_storage()
{
    thread_local thread_storage* thread_storage_ptr = nullptr;
    return &thread_storage_ptr;
}

inline thread_storage* get_this_thread_storage_ptr()
{
    return *this_thread_storage();
}

inline void set_this_thread_storage_ptr(thread_storage* thread_ptr)
{
    *this_thread_storage() = thread_ptr;
}

inline thread_storage& this_thread_storage_ref()
{
    thread_storage* ptr = get_this_thread_storage_ptr();
    if (ptr == nullptr)
        throw std::runtime_error("thread_storage only exists inside event loop");

    return *ptr;
}

}  // namespace co::impl