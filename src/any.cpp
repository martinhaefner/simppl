#include "simppl/any.h"
#include "simppl/pod.h"
#include "simppl/serialization.h"
#include "simppl/string.h"
#include "simppl/vector.h"
#include <any>
#include <cassert>
#include <cstdint>
#include <dbus/dbus-protocol.h>
#include <dbus/dbus.h>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace simppl {
namespace dbus {
std::any decodeAsAny(DBusMessageIter &varIter, int type) {
  switch (type) {
  case DBUS_TYPE_INT32:
    return decode2<int32_t>(varIter);
  case DBUS_TYPE_STRING:
    return decode2<std::string>(varIter);
  case DBUS_TYPE_VARIANT:
    return decode2<Any>(varIter);
  case DBUS_TYPE_ARRAY:
    return decode2<AnyVec>(varIter);
  default:
    assert(false);
  }
}

void encodeAny(DBusMessageIter &iter, const std::any &any, const int anyType) {
  switch (anyType) {
  case DBUS_TYPE_INT32:
    encode2(iter, std::any_cast<uint32_t>(any));
  case DBUS_TYPE_STRING:
    encode2(iter, std::any_cast<std::string>(any));
  case DBUS_TYPE_VARIANT:
    encode2(iter, std::any_cast<Any>(any));
  case DBUS_TYPE_ARRAY:
    if (any.type() == typeid(AnyVec)) {
      encode2(iter, std::any_cast<AnyVec>(any));
    } else {
      assert(false);
    }
  default:
    assert(false);
  }
}

void getSignatureFromBasicType(std::ostream &os, int type) {
  switch (type) {
  case DBUS_TYPE_INT32:
    Codec<int32_t>::make_type_signature(os);
    break;
  case DBUS_TYPE_STRING:
    Codec<std::string>::make_type_signature(os);
    break;
  case DBUS_TYPE_VARIANT:
    Codec<Any>::make_type_signature(os);
    break;
  default:
    assert(false);
  }
}

template <> AnyVec decode2(DBusMessageIter &iter) {
  DBusMessageIter arrayIter;
  dbus_message_iter_recurse(&iter, &arrayIter);

  std::vector<std::any> array;
  int elementType = dbus_message_iter_get_arg_type(&arrayIter);
  std::string elementSignature = dbus_message_iter_get_signature(&arrayIter);
  assert(elementType !=
         DBUS_TYPE_INVALID); // Else the message is invalid. Every empty array
                             // has to include the contained type
  if (elementType != DBUS_TYPE_INVALID) {
    do {
      array.emplace_back(decodeAsAny(arrayIter, elementType));
    } while (dbus_message_iter_next(&arrayIter));
  }
  return AnyVec{elementType, std::move(elementSignature), std::move(array)};
}

template <> Any decode2(DBusMessageIter &iter) {
  DBusMessageIter varIter;
  dbus_message_iter_recurse(&iter, &varIter);
  std::string elementSignature = dbus_message_iter_get_signature(&varIter);

  int type = dbus_message_iter_get_arg_type(&varIter);
  return {type, std::move(elementSignature), decodeAsAny(varIter, type)};
}

template <> std::string decode2(DBusMessageIter &iter) {
  char *value;
  dbus_message_iter_get_basic(&iter, &value);
  return std::string(value);
}

template <typename T> T decode2(DBusMessageIter &iter) {
  T val;
  Codec<T>::decode(iter, val);
  return val;
}

template <typename T> void encode2(DBusMessageIter &iter, const T &val) {
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &val);
}

template <> void encode2(DBusMessageIter &iter, const std::string &val) {
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, val.c_str());
}

template <> void encode2(DBusMessageIter &iter, const AnyVec &val) {
  DBusMessageIter arrayIter;
  std::cout << "encode2(AnyVec): " << val.elementSignature << "\n";
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, val.elementSignature.c_str(),
                                   &arrayIter);
  for (const std::any &elem : val.vec) {
    encodeAny(iter, elem, val.elementType);
  }
  dbus_message_iter_close_container(&iter, &arrayIter);
}

template <> void encode2(DBusMessageIter &iter, const Any &val) {
  std::ostringstream os;
  Codec<Any>::make_type_signature(os);

  DBusMessageIter variantIter;
  std::cout << "encode2(Any): " << os.str() << "\n";
  dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, os.str().c_str(),
                                   &variantIter);
  encodeAny(iter, val.value_, val.containedType);
  dbus_message_iter_close_container(&iter, &variantIter);
}
} // namespace dbus
} // namespace simppl