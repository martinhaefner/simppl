#ifndef SIMPPL_ANY_H
#define SIMPPL_ANY_H

#include <any>
#include <bits/utility.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <dbus/dbus-protocol.h>
#include <memory>
#include <sstream>
#include <utility>
#include <variant>
#include <iostream>

#include <dbus/dbus.h>
#include <vector>

#include "simppl/detail/deserialize_and_return.h"
#include "simppl/pod.h"
#include "simppl/serialization.h"
#include "simppl/type_mapper.h"

namespace simppl {

namespace dbus {
struct Any;
struct IntermediateAnyVec;
struct IntermediateAnyMap;
struct IntermediateAnyTuple;

template <typename T> T decode2(DBusMessageIter &iter);
template <> IntermediateAnyVec decode2(DBusMessageIter &iter);
template <> IntermediateAnyMap decode2(DBusMessageIter &iter);
template <> IntermediateAnyTuple decode2(DBusMessageIter &iter);
template <> Any decode2(DBusMessageIter &iter);
template <> std::string decode2(DBusMessageIter &iter);

template <typename T> void encode2(DBusMessageIter &iter, const T &val);
template <> void encode2(DBusMessageIter &iter, const IntermediateAnyVec &val);
template <> void encode2(DBusMessageIter &iter, const IntermediateAnyMap &val);
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

  Any &operator=(Any &&old) {
    containedType = old.containedType;
    containedTypeSignature = std::move(old.containedTypeSignature);
    value_ = std::move(old.value_);
    return *this;
  }

  Any &operator=(const Any &other) {
    containedType = other.containedType;
    containedTypeSignature = other.containedTypeSignature;
    value_ = other.value_;
    return *this;
  }

  bool operator==(const Any &other) const = delete;

  template <typename T> T as() const;
  template <typename T> bool is() const;
};

/**
 * A vector that contain anything and acts as an intermediate representation for
 *data stored inside simppl::dbus::Any.
 **/
struct IntermediateAnyVec {
  // DBUS_TYPE_...
  int elementType{DBUS_TYPE_INVALID};
  std::string elementSignature;
  std::vector<std::any> vec;
};

struct IntermediateAnyMap {
  // DBUS_TYPE_...
  int keyType{DBUS_TYPE_INVALID};
  // DBUS_TYPE_...
  int elementType{DBUS_TYPE_INVALID};
  std::string elementSignature;
  std::vector<std::tuple<std::any, std::any>> map;
};

struct IntermediateAnyTuple {
  // DBUS_TYPE_...
  std::vector<int> elementTypes;
  std::string elementsSignature;
  std::vector<std::any> elements;
};

// ---------------------------------------DBUS-TYPE----------------------------------------

template <> int get_debus_type<Any>();

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
    assert(false);
    return os << DBUS_TYPE_ARRAY_AS_STRING;
  }
};

template <> struct Codec<IntermediateAnyMap> {
  static void encode(DBusMessageIter &iter, const IntermediateAnyMap &v) {
    encode2(iter, v);
  }

  static void decode(DBusMessageIter &orig, IntermediateAnyMap &v) {
    v = decode2<IntermediateAnyMap>(orig);
    dbus_message_iter_next(&orig);
  }

  static inline std::ostream &make_type_signature(std::ostream &os) {
    assert(false);
    return os << DBUS_TYPE_ARRAY_AS_STRING;
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
    assert(false);
    return os << DBUS_TYPE_STRUCT_AS_STRING;
  }
};

// ---------------------------------------TYPE-CHECK---------------------------------------

template <typename T> struct is_type {
  static bool check(const std::any &any) {
    if (any.type() == typeid(Any)) {
      return is_type<T>::check(std::any_cast<Any>(any).value_);
    }
    if (any.type() == typeid(IntermediateAnyVec)) {
      return false;
    }
    return any.type() == typeid(T);
  }
};

template <> struct is_type<Any> {
  static bool check(const std::any &any) {
    if (any.type() == typeid(Any)) {
      return true;
    }
    return false;
  }
};

template <typename T, typename Alloc> struct is_type<std::vector<T, Alloc>> {
  static bool check(const std::any &any) {
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
check_tuple_rec(const std::vector<std::any> &elements) {
  return true;
}

// Recursive case
template <size_t I = 0, typename... Types>
inline std::enable_if_t<I < sizeof...(Types), bool>
check_tuple_rec(const std::vector<std::any> &elements) {
  if (I >= elements.size()) {
    return false;
  }

  using T = std::tuple_element_t<I, std::tuple<Types...>>;
  if(!is_type<T>::check(elements[I])) {
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

template <typename T> bool Any::is() const { return is_type<T>::check(value_); }

// --------------------------------------TYPE-CONVERT--------------------------------------

template <typename T> struct as_type {
  static T convert(const std::any &any) {
    assert(any.type() != typeid(IntermediateAnyVec));
    assert(any.type() != typeid(IntermediateAnyTuple));
    assert(any.type() != typeid(IntermediateAnyMap));

    if (any.type() == typeid(Any)) {
      return as_type<T>::convert(std::any_cast<Any>(any).value_);
    }
    return std::any_cast<T>(any);
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
                  std::tuple<Types...> &result) {
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

template <> struct get_signature<Any> {
  static void value(std::ostream &os, const Any & /*any*/) {
    os << DBUS_TYPE_VARIANT;
  }
};

template <typename T> std::string get_signature_func(const T &t) {
  std::ostringstream os;
  get_signature<typename std::remove_cv<
      typename std::remove_reference<T>::type>::type>::value(os, t);
  return os.str();
}

// --------------------------------INTERMEDIATE-CONVERSION---------------------------------

template <typename T> std::any to_intermediate(const T &t) { return t; }
template <typename T, typename Alloc>
std::any to_intermediate(const std::vector<T, Alloc> &vec);
template <typename... Types>
std::any to_intermediate(const std::tuple<Types...> &tuple);

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
to_intermediate_tuple_rec(const std::tuple<Types...> &tuple,
                          std::vector<std::any> &tupleVec,
                          std::vector<int> &tupleTypes, std::ostream &os) {
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
