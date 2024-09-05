#ifndef SIMPPL_ANY_H
#define SIMPPL_ANY_H

#include <any>
#include <cassert>
#include <cstring>
#include <dbus/dbus-protocol.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/types.h>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <variant>

#include <dbus/dbus.h>
#include <vector>

#include "simppl/detail/deserialize_and_return.h"
#include "simppl/map.h"
#include "simppl/pod.h"
#include "simppl/serialization.h"
#include "simppl/tuple.h"
#include "simppl/type_mapper.h"
#include "simppl/variant.h"
#include "simppl/vector.h"
#include "simppl/struct.h"

namespace simppl {

namespace dbus {
struct Any;
struct IntermediateAnyVec;
struct IntermediateAnyMapElement;
struct IntermediateAnyTuple;

template <typename T> T decode2(DBusMessageIter &iter);
template <> IntermediateAnyVec decode2(DBusMessageIter &iter);
template <> IntermediateAnyMapElement decode2(DBusMessageIter &iter);
template <> IntermediateAnyTuple decode2(DBusMessageIter &iter);
template <> Any decode2(DBusMessageIter &iter);
template <> std::string decode2(DBusMessageIter &iter);

template <typename T> void encode2(DBusMessageIter &iter, const T &val);
template <> void encode2(DBusMessageIter &iter, const IntermediateAnyVec &val);
template <>
void encode2(DBusMessageIter &iter, const IntermediateAnyMapElement &val);
template <>
void encode2(DBusMessageIter &iter, const IntermediateAnyTuple &val);
template <> void encode2(DBusMessageIter &iter, const Any &val);
template <> void encode2(DBusMessageIter &iter, const std::string &val);

/**
 * A true D-Bus variant that can really hold *anything*, as a replacement
 * for variants.
 **/
struct Any {
  // DBUS_TYPE_...
  int containedType{DBUS_TYPE_INVALID};
  std::string containedTypeSignature;
  std::any value_;

  Any(){};

  Any(int type, std::string &&containedTypeSignature, std::any &&any)
      : containedType(type),
        containedTypeSignature(std::move(containedTypeSignature)),
        value_(std::move(any)) {}

  Any(Any &&old);
  Any(const Any &other);

  template <typename T> Any(T &&value);
  template <typename T> Any(const T &value);

  template <typename T> Any &operator=(T &&val);
  template <typename T> Any &operator=(const T &val);

  Any &operator=(Any &&old);
  Any &operator=(const Any &other);

  bool operator==(const Any &other) const = delete;
  bool operator!=(const Any &other) const = delete;

  template <typename T> T as() const;
  template <typename T> bool is() const;
};

/**
 * A vector that can contain anything and acts as an intermediate representation
 * for data stored inside simppl::dbus::Any.
 **/
struct IntermediateAnyVec {
  // DBUS_TYPE_...
  int elementType{DBUS_TYPE_INVALID};
  std::string elementSignature;
  std::vector<std::any> vec;
};

/**
 * A map element that can contain anything and acts as an intermediate
 * representation for data stored inside simppl::dbus::Any.
 **/
struct IntermediateAnyMapElement {
  // DBUS_TYPE_...
  int keyType{DBUS_TYPE_INVALID};
  // DBUS_TYPE_...
  int valueType{DBUS_TYPE_INVALID};
  std::string keyValueSignature;
  std::any key;
  std::any value;
};

/**
 * A tuple that can contain anything and acts as an intermediate representation
 * for data stored inside simppl::dbus::Any.
 **/
struct IntermediateAnyTuple {
  // DBUS_TYPE_...
  std::vector<int> elementTypes;
  std::string elementsSignature;
  std::vector<std::any> elements;
};

// ---------------------------------------CODEC-IMPL---------------------------------------

template <> struct Codec<Any> {
  static void encode(DBusMessageIter &iter, const Any &v) { encode2(iter, v); }

  static void decode(DBusMessageIter &orig, Any &v) {
    v = decode2<Any>(orig);
    dbus_message_iter_next(&orig);
  }

  static inline std::ostream &make_type_signature(std::ostream &os) {
    return os << DBUS_TYPE_VARIANT_AS_STRING;
  }
};

template <> struct Codec<IntermediateAnyVec> {
  static void encode(DBusMessageIter &iter, const IntermediateAnyVec &v) {
    encode2(iter, v);
  }

  static void decode(DBusMessageIter &orig, IntermediateAnyVec &v) {
    v = decode2<IntermediateAnyVec>(orig);
    dbus_message_iter_next(&orig);
  }

  static inline std::ostream &make_type_signature(std::ostream &os) {
    throw std::logic_error(
        "Do not use this function. Use IntermediateAnyVec::elementSignature or "
        "get_signature<T>::value() instead.");
    return os << DBUS_TYPE_ARRAY_AS_STRING;
  }
};

template <> struct Codec<IntermediateAnyMapElement> {
  static void encode(DBusMessageIter &iter,
                     const IntermediateAnyMapElement &v) {
    encode2(iter, v);
  }

  static void decode(DBusMessageIter &orig, IntermediateAnyMapElement &v) {
    v = decode2<IntermediateAnyMapElement>(orig);
    dbus_message_iter_next(&orig);
  }

  static inline std::ostream &make_type_signature(std::ostream &os) {
    throw std::logic_error("Do not use this function. Use "
                           "IntermediateAnyMapElement::elementSignature or "
                           "get_signature<T>::value() instead.");
    return os << DBUS_TYPE_DICT_ENTRY_AS_STRING;
  }
};

template <> struct Codec<IntermediateAnyTuple> {
  static void encode(DBusMessageIter &iter, const IntermediateAnyTuple &v) {
    encode2(iter, v);
  }

  static void decode(DBusMessageIter &orig, IntermediateAnyTuple &v) {
    v = decode2<IntermediateAnyTuple>(orig);
    dbus_message_iter_next(&orig);
  }

  static inline std::ostream &make_type_signature(std::ostream &os) {
    throw std::logic_error(
        "Do not use this function. Use IntermediateAnyTuple::elementsSignature "
        "or get_signature<T>::value() instead.");
    return os << DBUS_TYPE_STRUCT_AS_STRING;
  }
};

// ---------------------------------------TYPE-CHECK---------------------------------------

template <typename T> struct is_type;
template <> struct is_type<Any>;
template <typename T, typename Alloc> struct is_type<std::vector<T, Alloc>>;
template <typename... Types> struct is_type<std::tuple<Types...>>;
template <typename... Types> struct is_type<std::variant<Types...>>;
template <typename Key, typename T, typename Compare, typename Allocator>
struct is_type<std::map<Key, T, Compare, Allocator>>;

template<typename T>
struct get_underlying_type
{
	// enum to int mapping is simppl specific
	using type = typename std::conditional_t<std::is_enum_v<T>, int, T>;
};

template <typename T> struct is_type {
  static bool check(const std::any &any) {
    if (any.type() == typeid(Any)) {
      return is_type<T>::check(std::any_cast<Any>(any).value_);
    }
    if (any.type() == typeid(IntermediateAnyVec)) {
      return false;
    }
    return any.type() == typeid(typename get_underlying_type<T>::type);
  }
};

template <> struct is_type<Any> {
  static bool check(const std::any &any) { return any.type() == typeid(Any); }
};

template <typename T, typename Alloc> struct is_type<std::vector<T, Alloc>> {
  static bool check(const std::any &any) {
    if (any.type() == typeid(Any)) {
      return is_type<std::vector<T, Alloc>>::check(std::any_cast<Any>(any).value_);
    }
    if (any.type() != typeid(IntermediateAnyVec)) {
      return false;
    }

    for (const std::any &elem : std::any_cast<IntermediateAnyVec>(any).vec) {
      if (!is_type<T>::check(elem)) {
        return false;
      }
    }
    return true;
  }
};

// Base case: Last type
template <size_t I = 0, typename... Types>
inline std::enable_if_t<I >= sizeof...(Types), bool>
check_tuple_rec(const std::vector<std::any> & /*elements*/) {
  return true;
}

// Recursive case
template <size_t I = 0, typename... Types>
    inline std::enable_if_t <
    I<sizeof...(Types), bool>
    check_tuple_rec(const std::vector<std::any> &elements) {
  if (I >= elements.size()) {
    return false;
  }

  using T = std::tuple_element_t<I, std::tuple<Types...>>;
  if (!is_type<T>::check(elements[I])) {
    return false;
  }
  return check_tuple_rec<I + 1, Types...>(elements);
}

template <typename... Types> struct is_type<std::tuple<Types...>> {
  static bool check(const std::any &any) {
    if (any.type() != typeid(IntermediateAnyTuple)) {
      return false;
    }

    return check_tuple_rec<0, Types...>(
        std::any_cast<IntermediateAnyTuple>(any).elements);
  }
};

// Base case: Last type
template <size_t I = 0, typename... Types>
inline std::enable_if_t<I >= sizeof...(Types), bool>
check_variant_rec(const std::any & /*any*/) {
  return false;
}

// Recursive case
template <size_t I = 0, typename... Types>
    inline std::enable_if_t <
    I<sizeof...(Types), bool> check_variant_rec(const std::any &any) {
  using T = std::tuple_element_t<I, std::tuple<Types...>>;
  if (is_type<T>::check(any)) {
    return true;
  }
  return check_variant_rec<I + 1, Types...>(any);
}

template <typename... Types> struct is_type<std::variant<Types...>> {
  static bool check(const std::any &any) {
    if (any.type() != typeid(Any)) {
      return false;
    }

    // Unwrap any in any in any in ...
    std::any anyInner = std::any_cast<Any>(any).value_;
    if (anyInner.type() == typeid(Any)) {
      return is_type<std::variant<Types...>>::check(
          std::any_cast<Any>(any).value_);
    }

    return check_variant_rec<0, Types...>(anyInner);
  }
};

template <typename Key, typename T, typename Compare, typename Allocator>
struct is_type<std::map<Key, T, Compare, Allocator>> {
  static bool check(const std::any &any) {
    if (any.type() != typeid(IntermediateAnyVec)) {
      return false;
    }

    const IntermediateAnyVec &mapVec = std::any_cast<IntermediateAnyVec>(any);
    if (mapVec.elementType != DBUS_TYPE_DICT_ENTRY) {
      return false;
    }

    for (const std::any &elem : mapVec.vec) {
      if (elem.type() != typeid(IntermediateAnyMapElement)) {
        return false;
      }

      const IntermediateAnyMapElement &mapElem =
          std::any_cast<IntermediateAnyMapElement>(elem);
      if (!is_type<Key>::check(mapElem.key)) {
        return false;
      }
      if (!is_type<T>::check(mapElem.value)) {
        return false;
      }
    }
    return true;
  }
};

template <typename T> bool Any::is() const { return is_type<T>::check(value_); }

// --------------------------------------TYPE-CONVERT--------------------------------------

template <typename T> struct as_type {
  static T convert(const std::any &any) {
    assert(any.type() != typeid(IntermediateAnyVec));
    assert(any.type() != typeid(IntermediateAnyTuple));
    assert(any.type() != typeid(IntermediateAnyMapElement));

    if (any.type() == typeid(Any)) {
      return as_type<T>::convert(std::any_cast<Any>(any).value_);
    }
    return static_cast<T>(std::any_cast<typename get_underlying_type<T>::type>(any));
  }
};

template <> struct as_type<Any> {
  static Any convert(const std::any &any) {
    assert(any.type() == typeid(Any));
    return std::any_cast<Any>(any);
  }
};

template <typename T, typename Alloc> struct as_type<std::vector<T, Alloc>> {
  static std::vector<T, Alloc> convert(const std::any &any) {
    if (any.type() == typeid(Any)) {
      return as_type<std::vector<T, Alloc>>::convert(std::any_cast<Any>(any).value_);
    }
    assert(any.type() == typeid(IntermediateAnyVec));

    std::vector<T, Alloc> result;
    for (const std::any &elem : std::any_cast<IntermediateAnyVec>(any).vec) {
      result.emplace_back(as_type<T>::convert(elem));
    }
    return result;
  }
};

// Base case: Last type
template <size_t I = 0, typename... Types>
inline std::enable_if_t<(I >= sizeof...(Types)), void>
convert_tuple_rec(const std::vector<std::any> &elements,
                  std::tuple<Types...> &/*result*/) {
  assert(I == elements.size());
}

// Recursive case
template <size_t I = 0, typename... Types>
inline std::enable_if_t<(I < sizeof...(Types)), void>
convert_tuple_rec(const std::vector<std::any> &elements,
                  std::tuple<Types...> &result) {
  using T = std::tuple_element_t<I, std::tuple<Types...>>;
  assert(I < elements.size());

  std::get<I>(result) = as_type<T>::convert(elements[I]);
  convert_tuple_rec<I + 1, Types...>(elements, result);
}

template <typename... Types> struct as_type<std::tuple<Types...>> {
  static std::tuple<Types...> convert(const std::any &any) {
    assert(any.type() == typeid(IntermediateAnyTuple));

    std::tuple<Types...> result;
    convert_tuple_rec<0, Types...>(
        std::any_cast<IntermediateAnyTuple>(any).elements, result);
    return result;
  }
};

// Base case: Last type
template <size_t I = 0, typename... Types>
inline std::enable_if_t<(I >= sizeof...(Types)), void>
convert_variant_rec(const Any &any, std::variant<Types...> & /*result*/) {
  throw std::invalid_argument(
      "std::variant does not contain the required type '" +
      any.containedTypeSignature + "'.");
}

// Recursive case
template <size_t I = 0, typename... Types>
inline std::enable_if_t<(I < sizeof...(Types)), void>
convert_variant_rec(const Any &any, std::variant<Types...> &result) {
  using T = std::tuple_element_t<I, std::tuple<Types...>>;
  if (is_type<T>::check(any.value_)) {
    result = as_type<T>::convert(any.value_);
    return;
  }
  convert_variant_rec<I + 1, Types...>(any, result);
}

template <typename... Types> struct as_type<std::variant<Types...>> {
  static std::variant<Types...> convert(const std::any &any) {
    assert(any.type() == typeid(Any));

    // Unwrap any in any in any in ...
    std::any anyInner = std::any_cast<Any>(any).value_;
    if (anyInner.type() == typeid(Any)) {
      return is_type<std::variant<Types...>>::check(
          std::any_cast<Any>(any).value_);
    }

    std::variant<Types...> result;
    convert_variant_rec<0, Types...>(std::any_cast<Any>(any), result);
    return result;
  }
};

template <typename Key, typename T, typename Compare, typename Allocator>
struct as_type<std::map<Key, T, Compare, Allocator>> {
  static std::map<Key, T, Compare, Allocator> convert(const std::any &any) {
    assert(any.type() == typeid(IntermediateAnyVec));

    std::map<Key, T, Compare, Allocator> result;
    for (const std::any &elem : std::any_cast<IntermediateAnyVec>(any).vec) {
      assert(elem.type() == typeid(IntermediateAnyMapElement));
      const IntermediateAnyMapElement &mapElem =
          std::any_cast<IntermediateAnyMapElement>(elem);
      const Key key = as_type<Key>::convert(mapElem.key);
      assert(result.find(key) == result.end());
      result[std::move(key)] = as_type<T>::convert(mapElem.value);
    }
    return result;
  }
};

template <typename T> T Any::as() const {
  assert(is<T>()); // Ensure the type matches before we convert.
  return as_type<T>::convert(value_);
}

// -------------------------------------DBUS-SIGNATURE-------------------------------------

template <typename T> struct get_signature {
  static void value(std::ostream &os, const T & /*t*/) {
    Codec<T>::make_type_signature(os);
  }
};

template <> struct get_signature<IntermediateAnyVec> {
  static void value(std::ostream &os, const IntermediateAnyVec &vec) {
    os << DBUS_TYPE_ARRAY_AS_STRING << vec.elementSignature;
  }
};

template <> struct get_signature<IntermediateAnyTuple> {
  static void value(std::ostream &os, const IntermediateAnyTuple &vec) {
    os << DBUS_STRUCT_BEGIN_CHAR_AS_STRING << vec.elementsSignature
       << DBUS_STRUCT_END_CHAR_AS_STRING;
  }
};

template <> struct get_signature<IntermediateAnyMapElement> {
  static void value(std::ostream &os,
                    const IntermediateAnyMapElement &mapElem) {
    os << DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING << mapElem.keyValueSignature
       << DBUS_DICT_ENTRY_END_CHAR_AS_STRING;
  }
};

template <> struct get_signature<Any> {
  static void value(std::ostream &os, const Any & /*any*/) {
    os << DBUS_TYPE_VARIANT;
  }
};

template <typename T> std::string get_signature_func(const T &t) {
  std::ostringstream os;
  get_signature<typename std::remove_cv<
      typename std::remove_reference<T>::type>::type>::value(os, t);
  return std::move(os).str();
}

// --------------------------------INTERMEDIATE-CONVERSION---------------------------------

// For all types that are no structs and do not have the `serializer_type` declaration.
template <typename T, typename std::enable_if<!has_serializer_type<T>::value, int>::type = 0>
std::any to_intermediate(const T &t) { return t; }

template <typename T, typename Alloc>
std::any to_intermediate(const std::vector<T, Alloc> &vec);

template <typename... Types>
std::any to_intermediate(const std::tuple<Types...> &tuple);

template <typename... Types>
std::any to_intermediate(const std::variant<Types...> &variant);

template <typename Key, typename T, typename Compare, typename Allocator>
std::any to_intermediate(const std::map<Key, T, Compare, Allocator> &map);

// For all structs with the `serializer_type` declaration.
template <typename T, typename std::enable_if<has_serializer_type<T>::value, int>::type = 0>
std::any to_intermediate(const T &t) { return t; } // TODO continue with the intermediate representation here

template <typename T, typename Alloc>
std::any to_intermediate(const std::vector<T, Alloc> &vec) {
  std::vector<std::any> resultVec;
  int elementType = get_debus_type<T>();
  std::ostringstream os;
  Codec<T>::make_type_signature(os);
  std::string elementSignature = os.str();

  for (const T &t : vec) {
    resultVec.emplace_back(to_intermediate(t));
  }

  return IntermediateAnyVec{elementType, std::move(elementSignature),
                            std::move(resultVec)};
}

// Base case: Last type
template <size_t I = 0, typename... Types>
inline std::enable_if_t<(I == sizeof...(Types)), void>
to_intermediate_tuple_rec(const std::tuple<Types...> &/*tuple*/,
                          std::vector<std::any> &tupleVec,
                          std::vector<int> &tupleTypes, std::ostream &/*os*/) {
  assert((I == tupleVec.size()) && (I == tupleTypes.size()));
}

// Recursive case
template <size_t I = 0, typename... Types>
inline std::enable_if_t<(I < sizeof...(Types)), void>
to_intermediate_tuple_rec(const std::tuple<Types...> &tuple,
                          std::vector<std::any> &tupleVec,
                          std::vector<int> &tupleTypes, std::ostream &os) {
  using T = std::tuple_element_t<I, std::tuple<Types...>>;
  assert((I < tupleVec.size()) && (I < tupleTypes.size()));

  tupleTypes[I] = get_debus_type<T>();
  Codec<T>::make_type_signature(os);
  tupleVec[I] = to_intermediate(std::get<I>(tuple));

  to_intermediate_tuple_rec<I + 1, Types...>(tuple, tupleVec, tupleTypes, os);
}

template <typename... Types>
std::any to_intermediate(const std::tuple<Types...> &tuple) {
  std::vector<std::any> tupleVec;
  tupleVec.resize(std::tuple_size<std::tuple<Types...>>());

  std::vector<int> tupleTypes;
  tupleTypes.resize(std::tuple_size<std::tuple<Types...>>());

  std::ostringstream os;
  to_intermediate_tuple_rec<0, Types...>(tuple, tupleVec, tupleTypes, os);
  std::string elementSignature = os.str();

  return IntermediateAnyTuple{std::move(tupleTypes), elementSignature,
                              std::move(tupleVec)};
}

// Base case: Last type
template <size_t I = 0, typename... Types>
inline std::enable_if_t<(I >= sizeof...(Types)), Any>
to_intermediate_variant_rec(const std::variant<Types...> &/*variant*/) {
  throw std::invalid_argument(
      "std::variant does not contain the required type. This is an exception "
      "that should not happen since the std::variant obviously contains it. If "
      "you see it, please make a bug report.");
  return Any();
}

// Recursive case
template <size_t I = 0, typename... Types>
inline std::enable_if_t<(I < sizeof...(Types)), Any>
to_intermediate_variant_rec(const std::variant<Types...> &variant) {
  using T = std::tuple_element_t<I, std::tuple<Types...>>;

  if (std::holds_alternative<T>(variant)) {
    const T &t = std::get<T>(variant);

    std::ostringstream os;
    Codec<T>::make_type_signature(os);
    std::string elementSignature = os.str();

    return Any{get_debus_type<T>(), std::move(elementSignature),
               to_intermediate(t)};
  }
  return to_intermediate_variant_rec<I + 1, Types...>(variant);
}

template <typename... Types>
std::any to_intermediate(const std::variant<Types...> &variant) {
  return to_intermediate_variant_rec<0, Types...>(variant);
}

template <typename Key, typename T, typename Compare, typename Allocator>
std::any to_intermediate(const std::map<Key, T, Compare, Allocator> &map) {
  std::ostringstream os;
  os << DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING;
  Codec<Key>::make_type_signature(os);
  Codec<T>::make_type_signature(os);
  os << DBUS_DICT_ENTRY_END_CHAR_AS_STRING;
  std::string elementSignature = os.str();

  std::vector<std::any> mapVec;
  for (const std::pair<const Key, T> &pair : map) {
    std::any keyAny = to_intermediate(pair.first);
    std::any valueAny = to_intermediate(pair.second);
    mapVec.emplace_back(IntermediateAnyMapElement{
        get_debus_type<Key>(), get_debus_type<T>(), elementSignature,
        std::move(keyAny), std::move(valueAny)});
  }

  return IntermediateAnyVec{DBUS_TYPE_DICT_ENTRY, std::move(elementSignature),
                            std::move(mapVec)};
}

// -----------------------------------ANY-IMPLEMENTATIONS----------------------------------

template <typename T>
Any::Any(T &&value)
    : containedType(get_debus_type<T>()),
      containedTypeSignature(get_signature_func(value)),
      value_(to_intermediate(value)) {}

template <typename T>
Any::Any(const T &value)
    : containedType(get_debus_type<T>()),
      containedTypeSignature(get_signature_func(value)),
      value_(to_intermediate(value)) {}

template <typename T> Any &Any::operator=(T &&val) {
  containedType = get_debus_type<T>();
  containedTypeSignature = get_signature_func(val);
  value_ = to_intermediate(val);
  return *this;
}

template <typename T> Any &Any::operator=(const T &val) {
  containedType = get_debus_type<T>();
  containedTypeSignature = get_signature_func<T>(val);
  value_ = to_intermediate(val);
  return *this;
}
} // namespace dbus
} // namespace simppl

#endif // SIMPPL_ANY_H
