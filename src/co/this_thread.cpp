#include <co/this_thread.hpp>
#include <co/impl/thread_storage.hpp>

namespace co
{

const std::string& this_thread::name()
{
    return co::impl::this_thread_storage_ref().name;
}

uint64_t this_thread::id()
{
    return co::impl::this_thread_storage_ref().id;
}

co::stop_token this_thread::stop_token()
{
    return co::impl::this_thread_storage_ref().stop.get_token();
}

bool this_thread::stop_requested()
{
    return co::impl::this_thread_storage_ref().stop.stop_requested();
}

}