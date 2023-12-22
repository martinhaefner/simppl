#pragma once

#include "map.h"
#include "simppl/serialization.h"
#include "string.h"
#include "tuple.h"
#include "vector.h"
#include "struct.h"
#include <type_traits>
#include <dbus/dbus-protocol.h>

namespace simppl {
namespace dbus {
// For all structs with the `serializer_type` declaration.
template <typename T, typename std::enable_if<has_serializer_type<T>::value, int>::type = 0>
int get_debus_type() {
    return DBUS_TYPE_STRUCT;
}

// For everything else.
template <typename T, typename std::enable_if<!has_serializer_type<T>::value, int>::type = 0>
int get_debus_type() {
    return detail::typecode_switch<typename std::remove_cv<
        typename std::remove_reference<T>::type>::type>::value;
}
} // namespace dbus
} // namespace simppl