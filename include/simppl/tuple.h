#ifndef __SIMPPL_DBUS_TUPLE_H__
#define __SIMPPL_DBUS_TUPLE_H__


#include <tuple>


namespace simppl
{

namespace dbus
{

namespace detail
{


// FIXME remove this in favour of lambda expression!
template<typename SerializerT>
struct TupleSerializer // : noncopable
{
   inline
   TupleSerializer(SerializerT& s)
    : orig_(s)
    , s_(&iter_)
   {
      dbus_message_iter_open_container(orig_.iter_, DBUS_TYPE_STRUCT, nullptr, &iter_);
   }

   inline
   ~TupleSerializer()
   {
      dbus_message_iter_close_container(orig_.iter_, &iter_);
   }

   template<typename T>
   void operator()(const T& t);

   SerializerT& orig_;
   SerializerT s_;
   DBusMessageIter iter_;
};


template<typename DeserializerT>
struct TupleDeserializer // : noncopable
{
   inline
   TupleDeserializer(DeserializerT& s, bool flattened = false)
    : orig_(s)
    , s_(flattened?orig_.iter_:&iter_)
    , flattened_(flattened)
   {
      if (!flattened)
         dbus_message_iter_recurse(orig_.iter_, &iter_);
   }

   ~TupleDeserializer()
   {
      if (!flattened_)
         dbus_message_iter_next(orig_.iter_);
   }

   template<typename T>
   void operator()(T& t);

   DeserializerT& orig_;
   DeserializerT s_;
   DBusMessageIter iter_;
   bool flattened_;
};

   
template<typename... T>
struct Codec<std::tuple<T...>>
{
   static 
   void encode(Serializer& s, const std::tuple<T...>& t)
   {
      TupleSerializer<Serializer> ts(s);
      std_tuple_for_each(t, std::ref(ts));
   }
   
   
   static 
   void decode(Deserializer& s, std::tuple<T...>& t)
   {
      TupleDeserializer<Deserializer> tds(s);
      std_tuple_for_each(t, std::ref(tds));
   }
};


template<typename DeserializerT>
template<typename T>
inline
void TupleDeserializer<DeserializerT>::operator()(T& t)
{
   Codec<T>::decode(s_, t);
}


template<typename SerializerT>
template<typename T>
inline
void TupleSerializer<SerializerT>::operator()(const T& t)   // seems to be already a reference so no copy is done
{
   Codec<T>::encode(s_, t);
}


template<typename... T>
struct make_type_signature<std::tuple<T...>>
{
   // if templated lambdas are available this could be removed!
   struct helper
   {
      helper(std::ostream& os)
       : os_(os)
      {
         // NOOP
      }

      template<typename U>
      void operator()(const U&)
      {
         make_type_signature<U>::eval(os_);
      }

      std::ostream& os_;
   };

   static inline
   std::ostream& eval(std::ostream& os)
   {
      os << DBUS_STRUCT_BEGIN_CHAR_AS_STRING;
      std::tuple<T...> t;   // FIXME make this a type based version only, no value based iteration
      std_tuple_for_each(t, helper(os));
      os << DBUS_STRUCT_END_CHAR_AS_STRING;

      return os;
   }
};


template<typename... T>
struct dbus_type_code<std::tuple<T...>>              { enum { value = DBUS_TYPE_STRUCT }; };

   
}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // __SIMPPL_DBUS_TUPLE_H__
