#ifndef SIMPPL_DBUS_STRUCT_H
#define SIMPPL_DBUS_STRUCT_H


#include "simppl/serialization.h"

#if SIMPPL_HAVE_BOOST_FUSION
#   include <boost/fusion/adapted/struct/adapt_struct.hpp>
#   include <boost/fusion/support/is_sequence.hpp>
#   include <boost/fusion/algorithm.hpp>
#endif


namespace simppl
{

namespace dbus
{


template<typename T1, typename T2>
struct SerializerTuple : T1
{
   void encode(DBusMessageIter& iter) const
   {
      T1::encode(iter);
      Codec<T2>::encode(iter, data_);
   }


   void decode(DBusMessageIter& iter)
   {
      T1::decode(iter);
      Codec<T2>::decode(iter, data_);
   }


   static
   void make_type_signature(std::ostream& os)
   {
      T1::make_type_signature(os);
      Codec<T2>::make_type_signature(os);
   }

   T2 data_;
};


template<typename T>
struct SerializerTuple<T, NilType>
{
   void encode(DBusMessageIter& iter) const
   {
      Codec<T>::encode(iter, data_);
   }


   void decode(DBusMessageIter& iter)
   {
      Codec<T>::decode(iter, data_);
   }


   static
   void make_type_signature(std::ostream& os)
   {
      Codec<T>::make_type_signature(os);
   }

   T data_;
};


namespace detail
{


#if SIMPPL_HAVE_BOOST_FUSION

struct FusionEncoder
{
   explicit inline
   FusionEncoder(DBusMessageIter& iter)
    : iter_(iter)
   {
      // NOOP
   }

   template<typename T>
   inline
   void operator()(const T& t) const
   {
      Codec<T>::encode(iter_, t);
   }

   DBusMessageIter& iter_;
};


struct FusionDecoder
{
   explicit inline
   FusionDecoder(DBusMessageIter& iter)
    : iter_(iter)
   {
      // NOOP
   }

   template<typename T>
   inline
   void operator()(T& t) const
   {
      Codec<T>::decode(iter_, t);
   }

   DBusMessageIter& iter_;
};


struct FusionTypeWriter
{
   explicit inline
   FusionTypeWriter(std::ostream& os)
    : os_(os)
   {
      // NOOP
   }

   template<typename T>
   inline
   void operator()(const T&) const
   {
      Codec<T>::make_type_signature(os_);
   }

   std::ostream& os_;
};

#endif   // SIMPPL_HAVE_BOOST_FUSION


template<typename StructT, typename SelectorT>
struct StructSerializationHelper
{
   typedef typename StructT::serializer_type s_type;

   static
   void encode(DBusMessageIter& iter, const StructT& st);

   static
   void decode(DBusMessageIter& iter, const StructT& st);

   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      s_type::make_type_signature(os << DBUS_STRUCT_BEGIN_CHAR_AS_STRING);
      return os << DBUS_STRUCT_END_CHAR_AS_STRING;
   }
};


template<typename StructT, typename SelectorT>
void StructSerializationHelper<StructT, SelectorT>::encode(DBusMessageIter& iter, const StructT& st)
{
   DBusMessageIter _iter;
   dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &_iter);

   const s_type& tuple = *(s_type*)&st;
   tuple.encode(_iter);

   dbus_message_iter_close_container(&iter, &_iter);
}


template<typename StructT, typename SelectorT>
void StructSerializationHelper<StructT, SelectorT>::decode(DBusMessageIter& iter, const StructT& st)
{
   DBusMessageIter _iter;
   simppl_dbus_message_iter_recurse(&iter, &_iter, DBUS_TYPE_STRUCT);

   s_type& tuple = *(s_type*)&st;
   tuple.decode(_iter);

   dbus_message_iter_next(&iter);
}


#if SIMPPL_HAVE_BOOST_FUSION

template<typename StructT>
struct StructSerializationHelper<StructT, boost::mpl::true_>
{
   static inline
   void encode(DBusMessageIter& iter, const StructT& st)
   {
      DBusMessageIter _iter;
      dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &_iter);

      boost::fusion::for_each(st, FusionEncoder(_iter));

      dbus_message_iter_close_container(&iter, &_iter);
   }

   static inline
   void decode(DBusMessageIter& iter, StructT& st)
   {
      DBusMessageIter _iter;
      simppl_dbus_message_iter_recurse(&iter, &_iter, DBUS_TYPE_STRUCT);

      boost::fusion::for_each(st, FusionDecoder(_iter));

      dbus_message_iter_next(&iter);
   }

   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      os << DBUS_STRUCT_BEGIN_CHAR_AS_STRING;

      StructT* st = nullptr;
      boost::fusion::for_each(*st, FusionTypeWriter(os));

      return os << DBUS_STRUCT_END_CHAR_AS_STRING;
   }
};

#endif   // SIMPPL_HAVE_BOOST_FUSION

}   // namespace detail


template<typename T>
struct CodecImpl<T, Struct>
{
   static
   void encode(DBusMessageIter& iter, const T& st)
   {
      detail::StructSerializationHelper<T,
#if SIMPPL_HAVE_BOOST_FUSION
         typename boost::fusion::traits::is_sequence<T>::type
#else
         int
#endif
      >::encode(iter, st);
   }


   static
   void decode(DBusMessageIter& iter, T& st)
   {
      detail::StructSerializationHelper<T,
#if SIMPPL_HAVE_BOOST_FUSION
         typename boost::fusion::traits::is_sequence<T>::type
#else
         int /* just any type but mpl::true_*/
#endif
      >::decode(iter, st);
   }


   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      detail::StructSerializationHelper<T,
#if SIMPPL_HAVE_BOOST_FUSION
         typename boost::fusion::traits::is_sequence<T>::type
#else
         int /* just any type but mpl::true_*/
#endif
      >::make_type_signature(os);
      return os;
   }
};


namespace detail
{
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

}   // namespace detail


template<typename... T>
struct make_serializer
{
   typedef typename make_typelist<T...>::type type__;
   typedef typename detail::make_serializer_imp<type__>::type type;
};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_STRUCT_H
