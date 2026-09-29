#ifndef SIMPPL_DBUS_STRUCT_H
#define SIMPPL_DBUS_STRUCT_H


#include "simppl/serialization.h"

#if SIMPPL_HAVE_BOOST_FUSION
#   include <boost/fusion/adapted/struct/adapt_struct.hpp>
#   include <boost/fusion/support/is_sequence.hpp>
#   include <boost/fusion/algorithm.hpp>
#   include <boost/fusion/include/size.hpp>
#   include <boost/fusion/include/value_at.hpp>
#   include <utility>
#endif


namespace simppl
{

namespace dbus
{


template<typename T1, typename T2>
struct SerializerTuple : T1
{
   void encode(Encoder& e) const
   {
      T1::encode(e);
      detail::encode_one<T2>(e, data_);
   }


   void decode(Decoder& d)
   {
      T1::decode(d);
      detail::decode_one<T2>(d, data_);
   }

   T2 data_;
};


template<typename T>
struct SerializerTuple<T, NilType>
{
   void encode(Encoder& e) const
   {
      detail::encode_one<T>(e, data_);
   }


   void decode(Decoder& d)
   {
      detail::decode_one<T>(d, data_);
   }

   T data_;
};


namespace detail
{


/// signature of all members of a SerializerTuple, without the struct braces
template<typename SerializerT>
struct serializer_signature;

template<typename T>
struct serializer_signature<SerializerTuple<T, NilType>> : composite_signature<Codec<T>> {};

template<typename T1, typename T2>
struct serializer_signature<SerializerTuple<T1, T2>> : composite_signature<serializer_signature<T1>, Codec<T2>> {};


#if SIMPPL_HAVE_BOOST_FUSION

template<typename StructT, typename IndexSequenceT>
struct fusion_signature_impl;

template<typename StructT, std::size_t... I>
struct fusion_signature_impl<StructT, std::index_sequence<I...>>
 : composite_signature<
      signature_chars<DBUS_STRUCT_BEGIN_CHAR>,
      Codec<typename std::remove_cv<typename boost::fusion::result_of::value_at_c<StructT, I>::type>::type>...,
      signature_chars<DBUS_STRUCT_END_CHAR>>
{
};

template<typename StructT>
using fusion_signature = fusion_signature_impl<StructT, std::make_index_sequence<boost::fusion::result_of::size<StructT>::value>>;


struct FusionEncoder
{
   explicit inline
   FusionEncoder(Encoder& e)
    : e_(e)
   {
      // NOOP
   }

   template<typename T>
   inline
   void operator()(const T& t) const
   {
      detail::encode_one<T>(e_, t);
   }

   Encoder& e_;
};


struct FusionDecoder
{
   explicit inline
   FusionDecoder(Decoder& d)
    : d_(d)
   {
      // NOOP
   }

   template<typename T>
   inline
   void operator()(T& t) const
   {
      detail::decode_one<T>(d_, t);
   }

   Decoder& d_;
};


#endif   // SIMPPL_HAVE_BOOST_FUSION


template<typename StructT, typename SelectorT>
struct StructSerializationHelper
 : composite_signature<
      signature_chars<DBUS_STRUCT_BEGIN_CHAR>,
      serializer_signature<typename StructT::serializer_type>,
      signature_chars<DBUS_STRUCT_END_CHAR>>
{
   typedef typename StructT::serializer_type s_type;

   static
   void encode(Encoder& e, const StructT& st);

   static
   void decode(Decoder& d, const StructT& st);
};


template<typename StructT, typename SelectorT>
void StructSerializationHelper<StructT, SelectorT>::encode(Encoder& e, const StructT& st)
{
   Encoder members = e.open_container(DBUS_TYPE_STRUCT);

   const s_type& tuple = *(s_type*)&st;
   tuple.encode(members);
}


template<typename StructT, typename SelectorT>
void StructSerializationHelper<StructT, SelectorT>::decode(Decoder& d, const StructT& st)
{
   Decoder members = d.recurse(DBUS_TYPE_STRUCT);

   s_type& tuple = *(s_type*)&st;
   tuple.decode(members);

   d.next();
}


#if SIMPPL_HAVE_BOOST_FUSION

template<typename StructT>
struct StructSerializationHelper<StructT, boost::mpl::true_> : fusion_signature<StructT>
{
   static inline
   void encode(Encoder& e, const StructT& st)
   {
      Encoder members = e.open_container(DBUS_TYPE_STRUCT);

      boost::fusion::for_each(st, FusionEncoder(members));
   }

   static inline
   void decode(Decoder& d, StructT& st)
   {
      Decoder members = d.recurse(DBUS_TYPE_STRUCT);

      boost::fusion::for_each(st, FusionDecoder(members));

      d.next();
   }
};

#endif   // SIMPPL_HAVE_BOOST_FUSION


template<typename T>
using struct_selector_t =
#if SIMPPL_HAVE_BOOST_FUSION
   typename boost::fusion::traits::is_sequence<T>::type;
#else
   int;   /* just any type but mpl::true_*/
#endif


}   // namespace detail


template<typename T>
struct CodecImpl<T, Struct> : detail::StructSerializationHelper<T, detail::struct_selector_t<T>>
{
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
