#include "simppl/any.h"
#include "simppl/pod.h"
#include "simppl/serialization.h"
#include "simppl/string.h"
#include "simppl/type_mapper.h"
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
template <typename T> T decodeAsAny(DBusMessageIter &iter) {
  T val{};
  Codec<T>::decode(iter, val);
  return val;
}

std::any decodeAsAny(DBusMessageIter &iter, int type) {
  switch (type) {
  case DBUS_TYPE_BYTE:
    return decodeAsAny<uint8_t>(iter);
  case DBUS_TYPE_UINT16:
    return decodeAsAny<uint16_t>(iter);
  case DBUS_TYPE_INT16:
    return decodeAsAny<int16_t>(iter);
  case DBUS_TYPE_UINT32:
    return decodeAsAny<uint32_t>(iter);
  case DBUS_TYPE_INT32:
    return decodeAsAny<int32_t>(iter);
  case DBUS_TYPE_UINT64:
    return decodeAsAny<uint64_t>(iter);
  case DBUS_TYPE_INT64:
    return decodeAsAny<int64_t>(iter);
  case DBUS_TYPE_DOUBLE:
    return decodeAsAny<double>(iter);
  case DBUS_TYPE_STRING:
    return decodeAsAny<std::string>(iter);
  case DBUS_TYPE_VARIANT:
    return decode2<Any>(iter);
  case DBUS_TYPE_ARRAY:
    return decode2<AnyVec>(iter);
  default:
    assert(false);
  }
}

void encodeAny(DBusMessageIter &iter, const std::any &any, const int anyType) {
  switch (anyType) {
  case DBUS_TYPE_BYTE:
    Codec<uint8_t>::encode(iter, std::any_cast<uint8_t>(any));
    break;
  case DBUS_TYPE_UINT16:
    Codec<uint16_t>::encode(iter, std::any_cast<uint16_t>(any));
    break;
  case DBUS_TYPE_INT16:
    Codec<int16_t>::encode(iter, std::any_cast<int16_t>(any));
    break;
  case DBUS_TYPE_UINT32:
    Codec<uint32_t>::encode(iter, std::any_cast<uint32_t>(any));
    break;
  case DBUS_TYPE_INT32:
    Codec<int32_t>::encode(iter, std::any_cast<int32_t>(any));
    break;
  case DBUS_TYPE_UINT64:
    Codec<uint64_t>::encode(iter, std::any_cast<uint64_t>(any));
    break;
  case DBUS_TYPE_INT64:
    Codec<int64_t>::encode(iter, std::any_cast<int64_t>(any));
    break;
  case DBUS_TYPE_DOUBLE:
    Codec<double>::encode(iter, std::any_cast<double>(any));
    break;
  case DBUS_TYPE_STRING:
    Codec<std::string>::encode(iter, std::any_cast<std::string>(any));
    break;
  case DBUS_TYPE_VARIANT:
    encode2(iter, std::any_cast<Any>(any));
    break;
  case DBUS_TYPE_ARRAY:
    if (any.type() == typeid(AnyVec)) {
      encode2(iter, std::any_cast<AnyVec>(any));
    } else {
      assert(false); // Should not happen since we convert all vectors that get
                     // passed to Any to an intermediate representation
    }
    break;
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
    break;
  }
}

template <> AnyVec decode2(DBusMessageIter &iter) {
  int arrayElementType = dbus_message_iter_get_element_type(&iter);
  assert(
      arrayElementType !=
      DBUS_TYPE_INVALID); // If there is no element/type, the array is invalid.

  DBusMessageIter arrayIter;
  dbus_message_iter_recurse(&iter, &arrayIter);

  std::vector<std::any> array;
  std::string elementSignature = dbus_message_iter_get_signature(&arrayIter);
  for (int elementType = dbus_message_iter_get_arg_type(&arrayIter);
       elementType != DBUS_TYPE_INVALID;
       elementType = dbus_message_iter_get_arg_type(&arrayIter)) {
    array.emplace_back(decodeAsAny(arrayIter, elementType));
  }

  // Advance to next element
  dbus_message_iter_next(&iter);

  return AnyVec{arrayElementType, std::move(elementSignature),
                std::move(array)};
}

template <> Any decode2(DBusMessageIter &iter) {
  DBusMessageIter varIter;
  dbus_message_iter_recurse(&iter, &varIter);
  std::string elementSignature = dbus_message_iter_get_signature(&varIter);

  int type = dbus_message_iter_get_arg_type(&varIter);
  std::any any = decodeAsAny(varIter, type);

  // Advance to next element
  dbus_message_iter_next(&iter);

  return Any(type, std::move(elementSignature), std::move(any));
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
  dbus_message_iter_append_basic(&iter, get_debus_type<T>(), &val);
}

template <> void encode2(DBusMessageIter &iter, const std::string &val) {
  const char *s = val.c_str();
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &s);
}

template <> void encode2(DBusMessageIter &iter, const AnyVec &val) {
  DBusMessageIter arrayIter;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                                   val.elementSignature.c_str(), &arrayIter);
  for (const std::any &elem : val.vec) {
    encodeAny(arrayIter, elem, val.elementType);
  }
  dbus_message_iter_close_container(&iter, &arrayIter);
}

template <> void encode2(DBusMessageIter &iter, const Any &val) {
  if(val.containedType == DBUS_TYPE_INVALID) { throw std::invalid_argument("simppl::dbus::Any can not be empty!"); }
  DBusMessageIter variantIter;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
                                   val.containedTypeSignature.c_str(),
                                   &variantIter);
  encodeAny(variantIter, val.value_, val.containedType);
  dbus_message_iter_close_container(&iter, &variantIter);
}

Any::Any(Any &&old) {
  containedType = old.containedType;
  containedTypeSignature = std::move(old.containedTypeSignature);
  value_ = std::move(old.value_);
}

Any::Any(const Any &other) {
  containedType = other.containedType;
  containedTypeSignature = other.containedTypeSignature;
  value_ = other.value_;
}

template <> int get_debus_type<Any>() { return DBUS_TYPE_VARIANT; }
} // namespace dbus
} // namespace simppl