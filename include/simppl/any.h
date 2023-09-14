#ifndef SIMPPL_ANY_H
#define SIMPPL_ANY_H

#include <any>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <dbus/dbus-protocol.h>
#include <memory>
#include <sstream>
#include <utility>
#include <variant>

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

template <typename T> T decode2(DBusMessageIter &iter);
template <> IntermediateAnyVec decode2(DBusMessageIter &iter);
template <> Any decode2(DBusMessageIter &iter);
template <> std::string decode2(DBusMessageIter &iter);

template <typename T> void encode2(DBusMessageIter &iter, const T &val);
template <> void encode2(DBusMessageIter &iter, const IntermediateAnyVec &val);
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
 * A vector that contain anything and acts as an intermediate representation for data stored inside simppl::dbus::Any.
 **/
struct IntermediateAnyVec {
  // DBUS_TYPE_...
  int elementType{DBUS_TYPE_INVALID};
  std::string elementSignature;
  std::vector<std::any> vec;
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

template<> struct is_type<Any> {
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

template <typename T> bool Any::is() const {
  return is_type<T>::check(value_);
}

// --------------------------------------TYPE-CONVERT--------------------------------------

template <typename T> struct as_type {
  static T convert(const std::any &any) {
    if (any.type() == typeid(Any)) {
      return as_type<T>::convert(std::any_cast<Any>(any).value_);
    }
    if (any.type() == typeid(IntermediateAnyVec)) {
      assert(false);
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
    if (any.type() != typeid(IntermediateAnyVec)) {
      assert(false);
    }

    std::vector<T, Alloc> result;
    for (const std::any &elem : std::any_cast<IntermediateAnyVec>(any).vec) {
      result.emplace_back(as_type<T>::convert(elem));
    }
    return result;
  }
};

template <typename T> T Any::as() const { return as_type<T>::convert(value_); }

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
std::any to_intermediate(const std::vector<T, Alloc> &vec) {
  std::vector<std::any> resultVec;
  int elementType = get_debus_type<T>();
  std::ostringstream os;
  Codec<T>::make_type_signature(os);
  std::string elementSignature = os.str();

  for (const T &t : vec) {
    resultVec.emplace_back(to_intermediate(t));
  }

  return IntermediateAnyVec{elementType, std::move(elementSignature), std::move(resultVec)};
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
