#pragma once

#include "simppl/serialization.h"

namespace simppl {
namespace dbus {
template <typename T> int get_debus_type() {
  return detail::typecode_switch<typename std::remove_cv<
      typename std::remove_reference<T>::type>::type>::value;
}
} // namespace dbus
} // namespace simppl