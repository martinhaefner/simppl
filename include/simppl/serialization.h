#ifndef SIMPPL_SERIALIZATION_H
#define SIMPPL_SERIALIZATION_H


#include <cstdint>
#include <sstream>

#include <dbus/dbus.h>

#include "simppl/typelist.h"
#include "simppl/signature.h"


/// throwing exception if expected_type is not met.
void simppl_dbus_message_iter_recurse(DBusMessageIter* iter, DBusMessageIter* nested, int expected_type);

/// throwing exception if expected_type is not met; advancing iterator
void simppl_dbus_message_iter_get_basic(DBusMessageIter* iter, void* p, int expected_type);
      

namespace simppl
{
   
   
// get const removed from pointer; std::remove_const is not sufficient
template<typename T> struct remove_all_const : std::remove_const<T> {};

template<typename T> struct remove_all_const<T*> {
    typedef typename remove_all_const<T>::type *type;
};

template<typename T> struct remove_all_const<T * const> {
    typedef typename remove_all_const<T>::type *type;
};


namespace dbus
{


template<typename T>
struct isPod
{
   typedef make_typelist<
      int8_t,
      uint8_t,
      int16_t,
      uint16_t,
      int32_t,
      uint32_t,
      int64_t,
      uint64_t,
      float,
      double>::type pod_types;

  enum { value = Find<T, pod_types>::value >= 0 };
};


struct Pod;
struct Struct;

template<typename T, typename DeducerT>
struct CodecImpl;


template<typename T>
using deducer_type_t = typename std::conditional<isPod<T>::value || std::is_enum<T>::value, Pod, Struct>::type;


// type switch
template<typename T>
struct Codec : CodecImpl<T, deducer_type_t<T>>
{
   typedef deducer_type_t<T> deducer_type;

   typedef CodecImpl<T, deducer_type> impl_type;
};


inline
void encode(DBusMessageIter&)
{
   // NOOP
}


template<typename T1, typename... T>
inline
void encode(DBusMessageIter& iter, const T1& t1, const T&... t)
{
   Codec<typename simppl::remove_all_const<T1>::type>::encode(iter, t1);
   encode(iter, t...);
}


inline
void decode(DBusMessageIter&)
{
   // NOOP
}


template<typename T1, typename... T>
inline
void decode(DBusMessageIter& iter, T1& t1, T&... t)
{
   Codec<typename simppl::remove_all_const<T1>::type>::decode(iter, t1);
   decode(iter, t...);
}


class DecoderError : public std::exception
{
};


/**
 * Makes the message currently being decoded known to codecs which keep a
 * reference on it instead of copying the data (i.e. Any). Scopes nest,
 * the innermost wins. A scope with nullptr hides the enclosing ones.
 *
 * simppl sets up the scope for all messages it decodes. User code only
 * needs it when decoding own messages with simppl::dbus::decode(), without
 * a scope an Any keeps a private copy of its data.
 */
class DecodingScope
{
public:

   explicit
   DecodingScope(DBusMessage* msg);

   ~DecodingScope();

   DecodingScope(const DecodingScope&) = delete;
   DecodingScope& operator=(const DecodingScope&) = delete;

   /// @return the message of the innermost scope of the calling thread or nullptr
   static
   DBusMessage* current();

private:

   DBusMessage* prev_;
};


}   // namespace dbus

}   // namespace simppl


#include "simppl/bool.h"
#include "simppl/pod.h"


#endif   // SIMPPL_SERIALIZATION_H
