#ifndef SIMPPL_TYPE_WRAPPERS_H
#define SIMPPL_TYPE_WRAPPERS_H


#include <type_traits>


namespace simppl
{


/**
 * TODO this is obsolete with C++17
 */
template<bool B>
using bool_constant = std::integral_constant<bool, B>;


}   // namespace simppl


#endif  // SIMPPL_TYPE_WRAPPERS_H
