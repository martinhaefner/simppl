#ifndef SIMPPL_SERIALIZATION_H
#define SIMPPL_SERIALIZATION_H


#include <sstream>

#include <dbus/dbus.h>

#include "simppl/typelist.h"
#include "simppl/bool.h"


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
      char,
      signed char,
      unsigned char,
      short,
      unsigned short,
      int,
      unsigned int,
      long,
      unsigned long,
      long long,
      unsigned long long,
      float,
      double>::type pod_types;

  enum { value = Find<T, pod_types>::value >= 0 };
};


struct Pod;
struct Struct;

template<typename T, typename DeducerT>
struct CodecImpl;


// type switch
template<typename T>
struct Codec
{
   typedef typename std::conditional<isPod<T>::value || std::is_enum<T>::value, Pod, Struct>::type deducer_type;
      
   typedef CodecImpl<T, deducer_type> impl_type;
   
      
   static inline
   void encode(DBusMessageIter& s, const T& t)
   {
      impl_type::encode(s, t);
   }
   
   
   static inline
   void decode(DBusMessageIter& s, T& t)
   {
      impl_type::decode(s, t);
   }
   
   
   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return impl_type::make_type_signature(os);
   }
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


}   // namespace dbus

}   // namespace simppl



#include "simppl/pod.h"

#endif   // SIMPPL_SERIALIZATION_H
