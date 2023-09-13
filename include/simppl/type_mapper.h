#pragma once

#include "simppl/serialization.h"
#include <cassert>
#include <cstdint>
#include <dbus/dbus.h>
#include <sstream>
#include <string>
#include <vector>

namespace simppl {
namespace dbus {
template <typename T> struct dbus_type {
  static int value() {
    assert(false);
    return DBUS_TYPE_INVALID;
  }
};

template <> struct dbus_type<int32_t> {
  static int value() { return DBUS_TYPE_INT32; }
};

template <> struct dbus_type<std::string> {
  static int value() { return DBUS_TYPE_STRING; }
};

template <typename T, typename Alloc> struct dbus_type<std::vector<T, Alloc>> {
  static int value() { return DBUS_TYPE_ARRAY; }
};

template <typename T> int get_debus_type() {
  return dbus_type<typename std::remove_cv<
      typename std::remove_reference<T>::type>::type>::value();
}
} // namespace dbus
} // namespace simppl