#ifndef SIMPPL_DETAIL_SERIALIZATION_H
#define SIMPPL_DETAIL_SERIALIZATION_H


#include "simppl/noninstantiable.h"
#include "simppl/typelist.h"
#include "simppl/callstate.h"

#include <cxxabi.h>

#include <map>
#include <vector>
#include <tuple>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <tuple>
#include <sstream>
#include <stdint.h>

#include <dbus/dbus.h>

#ifdef SIMPPL_HAVE_BOOST_FUSION
#   include <boost/fusion/support/is_sequence.hpp>
#   include <boost/fusion/algorithm.hpp>
#endif


namespace simppl
{

namespace dbus
{

namespace detail
{
template<typename T>
struct Codec;

template<typename T>
struct make_type_signature;


// ---------------------------------------------------------------------


// generic type switch
template<typename T> struct dbus_type_code
{ enum { value = std::is_enum<T>::value ? DBUS_TYPE_INT32 : DBUS_TYPE_STRUCT }; };


// ---------------------------------------------------------------------


template<typename ListT>
struct make_serializer_imp;

template<typename T1, typename ListT>
struct make_serializer_imp<TypeList<T1, ListT> >
{
   typedef SerializerTuple<typename make_serializer_imp<TypeList<T1, typename PopBack<ListT>::type> >::type, typename Back<ListT>::type> type;
};

template<typename T>
struct make_serializer_imp<TypeList<T, NilType> >
{
   typedef SerializerTuple<T, NilType> type;
};


// ---------------------------------------------------------------------


template<typename T>
struct make_type_signature
{
   static inline std::ostream& eval(std::ostream& os)
   {
      make_type_signature<T>::helper(os, bool_constant<isPod<T>::value || std::is_enum<T>::value>());
      return os;
   }


private:

   // pod helper
   static inline void helper(std::ostream& os, std::true_type)
   {
      os << (char)dbus_type_code<T>::value;
   }

   // struct helper
   static inline void helper(std::ostream& os, std::false_type)
   {
      T::serializer_type::write_signature(os << DBUS_STRUCT_BEGIN_CHAR_AS_STRING);
      os << DBUS_STRUCT_END_CHAR_AS_STRING;
   }
};


// ---------------------------------------------------------------------


struct Pod {};
struct Pointer {};
struct Struct {};

template<typename T>
struct type_deducer
{
   typedef typename std::conditional<isPod<T>::value || std::is_enum<T>::value, Pod, 
      typename std::conditional<std::is_pointer<T>::value, Pointer, Struct>::type>::type type;
};


// ---------------------------------------------------------------------


struct Serializer // : noncopyable
{
   explicit
   Serializer(DBusMessage* msg);

   explicit
   Serializer(DBusMessageIter* iter);

   template<typename T>
   inline
   Serializer& write(const T& t)
   {
      return write(t, typename type_deducer<T>::type());
   }

   template<typename T>
   inline
   Serializer& write(T t, Pointer)
   {
      return write_ptr(t);
   }


private:

   DBusMessageIter private_iter_;


public:

   DBusMessageIter* iter_;
};

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


// -----------------------------------------------------------------------------


namespace simppl
{

namespace dbus
{

namespace detail
{

struct Deserializer // : noncopyable
{
   static inline
   void free(void* ptr)
   {
      delete[] (char*)ptr;
   }

   explicit
   Deserializer(DBusMessage* msg);

   explicit
   Deserializer(DBusMessageIter* iter);

   template<typename T>
   inline
   Deserializer& read(T& t)
   {
      return read(t, bool_constant<isPod<T>::value || std::is_enum<T>::value>());
   }
      
   
   template<typename... T>
   Deserializer& read_flattened(std::tuple<T...>& t)
   {
      TupleDeserializer<Deserializer> tds(*this, true);
      std_tuple_for_each(t, std::ref(tds));
      return *this;
   }

  
private:

   static inline
   void* allocate(size_t len)
   {
      return new char[len];
   }

   DBusMessageIter private_iter_;

public:   // FIXME friend?

   DBusMessageIter* iter_;
};

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


// ------------------------------------------------------------------------


namespace simppl
{

namespace dbus
{

namespace detail
{

inline
Serializer& serialize(Serializer& s)
{
   return s;
}

template<typename T1, typename... T>
inline
Serializer& serialize(Serializer& s, const T1& t1, const T&... t)
{
   Codec<T1>::encode(s, t1);
   return serialize(s, t...);
}


}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#include "simppl/string.h"
#include "simppl/buffer.h"
#include "simppl/vector.h"
#include "simppl/map.h"
#include "simppl/tuple.h"
#include "simppl/objectpath.h"
#include "simppl/variant.h"


namespace detail {

namespace dbus {

namespace simppl {


}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERIALIZATION_H
