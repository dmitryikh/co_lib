#pragma once

#include <boost/outcome.hpp>

#define CO_RESULT_TRYV BOOST_OUTCOME_CO_TRYV
#define CO_RESULT_TRY BOOST_OUTCOME_CO_TRY

namespace co
{

template <typename T>
using result = BOOST_OUTCOME_V2_NAMESPACE::result<T, std::error_code>;

#define ALIAS_TEMPLATE_FUNCTION(highLevelF, lowLevelF) \
template<typename... Args> \
inline auto highLevelF(Args&&... args) -> decltype(lowLevelF(std::forward<Args>(args)...)) \
{ \
    return lowLevelF(std::forward<Args>(args)...); \
}
ALIAS_TEMPLATE_FUNCTION(err, BOOST_OUTCOME_V2_NAMESPACE::failure)
ALIAS_TEMPLATE_FUNCTION(ok, BOOST_OUTCOME_V2_NAMESPACE::success)
// using err = BOOST_OUTCOME_V2_NAMESPACE::failure;
// using ok = BOOST_OUTCOME_V2_NAMESPACE::success;
#undef ALIAS_TEMPLATE_FUNCTION

}  // namespace co