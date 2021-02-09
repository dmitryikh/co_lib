#pragma once

#include <boost/intrusive/list.hpp>

namespace co::impl {

using intrusive_list_hook = boost::intrusive::list_member_hook<
    boost::intrusive::link_mode<boost::intrusive::auto_unlink>
>;

template <typename T, intrusive_list_hook T::*PtrToMember>
using intrusive_list = boost::intrusive::list<
    T,
    boost::intrusive::member_hook<T, intrusive_list_hook, PtrToMember>,
    boost::intrusive::constant_time_size<false>
>;

} // namespace co::impl