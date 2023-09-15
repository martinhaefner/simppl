#include "simppl/any.h"
#include "simppl/map.h"
#include "simppl/objectpath.h"
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
// ---------------------------------------DECODE-ANY---------------------------------------

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
  case DBUS_TYPE_OBJECT_PATH:
    return decodeAsAny<simppl::dbus::ObjectPath>(iter);
  case DBUS_TYPE_VARIANT:
    return decode2<Any>(iter);
  case DBUS_TYPE_ARRAY:
    return decode2<IntermediateAnyVec>(iter);
  case DBUS_TYPE_STRUCT:
    return decode2<IntermediateAnyTuple>(iter);
  case DBUS_TYPE_DICT_ENTRY:
    return decode2<IntermediateAnyMapElement>(iter);
  default:
    assert(false);
  }
}

template <> IntermediateAnyVec decode2(DBusMessageIter &iter) {
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

  return IntermediateAnyVec{arrayElementType, std::move(elementSignature),
                            std::move(array)};
}

template <> IntermediateAnyTuple decode2(DBusMessageIter &iter) {
  DBusMessageIter arrayIter;
  dbus_message_iter_recurse(&iter, &arrayIter);

  std::vector<std::any> tupleVec;
  std::vector<int> tupleTypes;
  std::string elementSignature = dbus_message_iter_get_signature(&arrayIter);
  for (int elementType = dbus_message_iter_get_arg_type(&arrayIter);
       elementType != DBUS_TYPE_INVALID;
       elementType = dbus_message_iter_get_arg_type(&arrayIter)) {
    tupleVec.emplace_back(decodeAsAny(arrayIter, elementType));
    tupleTypes.emplace_back(elementType);
  }

  // Advance to next element
  dbus_message_iter_next(&iter);

  return IntermediateAnyTuple{std::move(tupleTypes),
                              std::move(elementSignature), std::move(tupleVec)};
}

template <> IntermediateAnyMapElement decode2(DBusMessageIter &iter) {
  DBusMessageIter mapElemIter;
  dbus_message_iter_recurse(&iter, &mapElemIter);

  std::string keySignature = dbus_message_iter_get_signature(&mapElemIter);
  int keyType = dbus_message_iter_get_arg_type(&mapElemIter);
  std::any key = decodeAsAny(mapElemIter, keyType);

  std::string valueSignature = dbus_message_iter_get_signature(&mapElemIter);
  int valueType = dbus_message_iter_get_arg_type(&mapElemIter);
  std::any value = decodeAsAny(mapElemIter, valueType);

  // Advance to next element
  dbus_message_iter_next(&iter);

  return IntermediateAnyMapElement{keyType, valueType,
                                   keySignature + valueSignature,
                                   std::move(key), std::move(value)};
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

// ---------------------------------------ENCODE-ANY---------------------------------------

template <typename T>
void encodeAny(DBusMessageIter &iter, const std::any &any) {
  Codec<T>::encode(iter, std::any_cast<T>(any));
}

void encodeAny(DBusMessageIter &iter, const std::any &any, const int anyType) {
  switch (anyType) {
  case DBUS_TYPE_BYTE:
    encodeAny<uint8_t>(iter, any);
    break;
  case DBUS_TYPE_UINT16:
    encodeAny<uint16_t>(iter, any);
    break;
  case DBUS_TYPE_INT16:
    encodeAny<int16_t>(iter, any);
    break;
  case DBUS_TYPE_UINT32:
    encodeAny<uint32_t>(iter, any);
    break;
  case DBUS_TYPE_INT32:
    encodeAny<int32_t>(iter, any);
    break;
  case DBUS_TYPE_UINT64:
    encodeAny<uint64_t>(iter, any);
    break;
  case DBUS_TYPE_INT64:
    encodeAny<int64_t>(iter, any);
    break;
  case DBUS_TYPE_DOUBLE:
    encodeAny<double>(iter, any);
    break;
  case DBUS_TYPE_STRING:
    encodeAny<std::string>(iter, any);
    break;
  case DBUS_TYPE_OBJECT_PATH:
    encodeAny<simppl::dbus::ObjectPath>(iter, any);
    break;
  case DBUS_TYPE_VARIANT:
    encode2(iter, std::any_cast<Any>(any));
    break;
  case DBUS_TYPE_ARRAY:
    if (any.type() == typeid(IntermediateAnyVec)) {
      encode2(iter, std::any_cast<IntermediateAnyVec>(any));
    } else {
      assert(false); // Should not happen since we convert all vectors that get
                     // passed to Any to an intermediate representation.
    }
    break;
  case DBUS_TYPE_STRUCT:
    if (any.type() == typeid(IntermediateAnyTuple)) {
      encode2(iter, std::any_cast<IntermediateAnyTuple>(any));
    } else {
      assert(
          false); // Should not happen since we convert all tuples (structs)
                  // that get passed to Any to an intermediate representation.
    }
    break;
  case DBUS_TYPE_DICT_ENTRY:
    if (any.type() == typeid(IntermediateAnyMapElement)) {
      encode2(iter, std::any_cast<IntermediateAnyMapElement>(any));
    } else {
      assert(false); // Should not happen since we convert all map pairs that
                     // get passed to Any to an intermediate representation.
    }
    break;
  default:
    assert(false); // In case we fail here, we forgot an implementation for a
                   // dbus type. This is a bug then.
    break;
  }
}

template <typename T> void encode2(DBusMessageIter &iter, const T &val) {
  dbus_message_iter_append_basic(&iter, get_debus_type<T>(), &val);
}

template <> void encode2(DBusMessageIter &iter, const std::string &val) {
  const char *s = val.c_str();
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &s);
}

template <> void encode2(DBusMessageIter &iter, const IntermediateAnyVec &val) {
  DBusMessageIter arrayIter;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                                   val.elementSignature.c_str(), &arrayIter);
  for (const std::any &elem : val.vec) {
    encodeAny(arrayIter, elem, val.elementType);
  }
  dbus_message_iter_close_container(&iter, &arrayIter);
}

template <>
void encode2(DBusMessageIter &iter, const IntermediateAnyTuple &val) {
  DBusMessageIter structIter;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr,
                                   &structIter);
  for (size_t i = 0; i < val.elements.size(); i++) {
    encodeAny(structIter, val.elements[i], val.elementTypes[i]);
  }
  dbus_message_iter_close_container(&iter, &structIter);
}

template <>
void encode2(DBusMessageIter &iter, const IntermediateAnyMapElement &val) {
  DBusMessageIter mapElemIter;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                   &mapElemIter);
  encodeAny(mapElemIter, val.key, val.keyType);
  encodeAny(mapElemIter, val.value, val.valueType);
  dbus_message_iter_close_container(&iter, &mapElemIter);
}

template <> void encode2(DBusMessageIter &iter, const Any &val) {
  if (val.containedType == DBUS_TYPE_INVALID) {
    throw std::invalid_argument("simppl::dbus::Any can not be empty!");
  }
  DBusMessageIter variantIter;
  dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
                                   val.containedTypeSignature.c_str(),
                                   &variantIter);
  encodeAny(variantIter, val.value_, val.containedType);
  dbus_message_iter_close_container(&iter, &variantIter);
}

// -----------------------------------ANY-IMPLEMENTATIONS----------------------------------

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

// -----------------------------------MISC-IMPLEMENTATIONS---------------------------------

template <> int get_debus_type<Any>() { return DBUS_TYPE_VARIANT; }
} // namespace dbus
} // namespace simppl

/**
 *
  std::tuple<std::vector<uint16_t>,
             std::vector<std::tuple<uint64_t, std::string>>>
 *
 */