#pragma once

#include <string>
#include <co/impl/async_signal.hpp>
#include <co/impl/timer.hpp>
#include <co/stop_token.hpp>

namespace co::impl
{
class scheduler;

struct thread_storage
{
    std::string name;
    uint64_t id;
    stop_source stop;
    timer _timer{};
    async_signal async_signal{};
    // ptr to the scheduler which the co::thread belongs to
    scheduler* scheduler_ptr = nullptr;
    std::coroutine_handle<> suspended_coroutine = nullptr;
};

void wake_thread(thread_storage*);

inline std::shared_ptr<thread_storage> create_thread_storage(const std::string& thread_name,
                                                             uint64_t id,
                                                             scheduler* scheduler_ptr)
{
    auto storage = std::make_shared<thread_storage>();
    storage->id = id;
    if (thread_name.empty())
        storage->name = "co::thread" + std::to_string(storage->id);
    else
        storage->name = thread_name;

    assert(scheduler_ptr != nullptr);
    storage->scheduler_ptr = scheduler_ptr;
    storage->async_signal.data = static_cast<void*>(storage.get());
    auto callback = [](void* data)
    {
        assert(data != nullptr);
        thread_storage* storage = static_cast<thread_storage*>(data);
        wake_thread(storage);
    };
    storage->async_signal.callback = callback;

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

// The thread storage ptr will be not set after this call.
inline void current_thread_on_suspend(std::coroutine_handle<> awaiting_coroutine)
{
    thread_storage* thread = get_this_thread_storage_ptr();
    assert(thread != nullptr);
    assert(thread->suspended_coroutine.address() == nullptr);
    thread->suspended_coroutine = awaiting_coroutine;
    *this_thread_storage() = nullptr;
}

inline void set_this_thread_storage_ptr(thread_storage* thread)
{
    *this_thread_storage() = thread;
}

inline void current_thread_on_resume(thread_storage* thread)
{
    assert(thread != nullptr);
    set_this_thread_storage_ptr(thread);

    // co::ts_event has wierd behaviour when it calls `current_thread_on_suspend`,
    // but then call current thread_on_resume immideately.
    // That leaves suspended_coroutine to be set and never be used.
    thread->suspended_coroutine = std::coroutine_handle<>{};
}

inline thread_storage& this_thread_storage_ref()
{
    thread_storage* ptr = get_this_thread_storage_ptr();
    if (ptr == nullptr)
        throw std::runtime_error("thread_storage only exists inside event loop");

    return *ptr;
}

}  // namespace co::impl