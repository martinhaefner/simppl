#pragma once

#include "simppl/serialization.h"
#include <cassert>
#include <cstdint>
#include <dbus/dbus.h>
#include <sstream>
#include <string>
#include <vector>
#include "simppl/pod.h"

namespace simppl {
namespace dbus {
template <typename T> int get_debus_type() {
  return detail::typecode_switch<typename std::remove_cv<
      typename std::remove_reference<T>::type>::type>::value;
}
} // namespace dbus
} // namespace simppl