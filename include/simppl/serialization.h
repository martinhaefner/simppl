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


class DecoderError : public std::exception
{
};


/**
 * Writes values into a message. An Encoder for a container closes the
 * container when it goes out of scope (or abandons it during stack
 * unwinding).
 */
class Encoder
{
public:

   /// append to the message
   explicit
   Encoder(DBusMessage* msg);

   /// append via an existing iterator, the iterator is advanced
   explicit
   Encoder(DBusMessageIter& iter);

   Encoder(const Encoder&) = delete;
   Encoder& operator=(const Encoder&) = delete;

   ~Encoder();

   void append_basic(int type, const void* value);

   void append_fixed_array(int element_type, const void* data, int n);

   /// @param contained_signature needed for arrays and variants only
   Encoder open_container(int type, const char* contained_signature = nullptr);

   /// escape hatch for direct libdbus calls
   DBusMessageIter& native()
   {
      return *iter_;
   }

private:

   Encoder(DBusMessageIter& parent, int type, const char* contained_signature);

   DBusMessageIter own_;
   DBusMessageIter* iter_;
   DBusMessageIter* parent_;   ///< set for containers only
   int uncaught_;
};


/**
 * Reads values from a message. Knows the message it reads from, so
 * codecs may keep a reference on it instead of copying data (i.e. Any).
 */
class Decoder
{
   struct recurse_tag {};

public:

   /// read the message from the beginning
   explicit
   Decoder(DBusMessage* msg);

   /**
    * Read via an existing iterator, the iterator is advanced. The message
    * the iterator belongs to may be unknown (nullptr).
    */
   explicit
   Decoder(DBusMessageIter& iter, DBusMessage* msg = nullptr);

   /// a copy is an independent reader at the same position
   Decoder(const Decoder& rhs);

   Decoder& operator=(const Decoder&) = delete;

   int arg_type() const
   {
      return dbus_message_iter_get_arg_type(iter_);
   }

   bool at_end() const
   {
      return arg_type() == DBUS_TYPE_INVALID;
   }

   void next()
   {
      dbus_message_iter_next(iter_);
   }

   /// read and advance, @throw DecoderError if the type does not match
   void get_basic(void* p, int expected_type);

   /// reader for the container at the current position, @throw DecoderError if the type does not match
   Decoder recurse(int expected_type);

   /// @return the message read from or nullptr if unknown
   DBusMessage* message() const
   {
      return msg_;
   }

   /// escape hatch for direct libdbus calls
   DBusMessageIter& native()
   {
      return *iter_;
   }

private:

   Decoder(Decoder& parent, recurse_tag);

   DBusMessageIter own_;
   DBusMessageIter* iter_;
   DBusMessage* msg_;
};


namespace detail
{

template<typename T, typename ArgT, typename = void>
struct has_encoder_interface : std::false_type {};

template<typename T, typename ArgT>
struct has_encoder_interface<T, ArgT, std::void_t<decltype(Codec<T>::encode(std::declval<Encoder&>(), std::declval<const ArgT&>()))>> : std::true_type {};

template<typename T, typename = void>
struct has_decoder_interface : std::false_type {};

template<typename T>
struct has_decoder_interface<T, std::void_t<decltype(Codec<T>::decode(std::declval<Decoder&>(), std::declval<T&>()))>> : std::true_type {};


template<typename T, typename ArgT>
[[deprecated("Codec<T>::encode(DBusMessageIter&, const T&) is deprecated, use encode(Encoder&, const T&) instead")]]
inline
void legacy_encode(Encoder& e, const ArgT& t)
{
   Codec<T>::encode(e.native(), t);
}


template<typename T>
[[deprecated("Codec<T>::decode(DBusMessageIter&, T&) is deprecated, use decode(Decoder&, T&) instead")]]
inline
void legacy_decode(Decoder& d, T& t)
{
   Codec<T>::decode(d.native(), t);
}


/// encode t with the codec of T; ArgT may differ, e.g. const char* for T = char*
template<typename T, typename ArgT = T>
inline
void encode_one(Encoder& e, const ArgT& t)
{
   if constexpr (has_encoder_interface<T, ArgT>::value)
   {
      Codec<T>::encode(e, t);
   }
   else
      legacy_encode<T, ArgT>(e, t);
}


template<typename T>
inline
void decode_one(Decoder& d, T& t)
{
   if constexpr (has_decoder_interface<T>::value)
   {
      Codec<T>::decode(d, t);
   }
   else
      legacy_decode<T>(d, t);
}

}   // namespace detail


template<typename... T>
inline
void encode(Encoder& e, const T&... t)
{
   (detail::encode_one<typename simppl::remove_all_const<T>::type, T>(e, t), ...);
}


template<typename... T>
inline
void decode(Decoder& d, T&... t)
{
   (detail::decode_one<typename simppl::remove_all_const<T>::type>(d, t), ...);
}


/// convenience for plain libdbus iterators
template<typename... T>
inline
void encode(DBusMessageIter& iter, const T&... t)
{
   Encoder e(iter);
   encode(e, t...);
}


/**
 * Convenience for plain libdbus iterators. The message is unknown here,
 * so a decoded Any keeps a private copy of its data.
 */
template<typename... T>
inline
void decode(DBusMessageIter& iter, T&... t)
{
   Decoder d(iter);
   decode(d, t...);
}


}   // namespace dbus

}   // namespace simppl


#include "simppl/bool.h"
#include "simppl/pod.h"


#endif   // SIMPPL_SERIALIZATION_H
