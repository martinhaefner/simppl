#ifndef SIMPPL_DBUS_TUPLE_H
#define SIMPPL_DBUS_TUPLE_H


#include <tuple>

#include "simppl/serialization.h"
#include "simppl/for_each.h"


namespace simppl
{

namespace dbus
{

namespace detail
{


struct TupleSerializer // : noncopable
{
   inline
   TupleSerializer(DBusMessageIter& iter)
    : orig_(iter)
   {
      dbus_message_iter_open_container(&orig_, DBUS_TYPE_STRUCT, nullptr, &iter_);
   }
   

   inline
   ~TupleSerializer()
   {
      dbus_message_iter_close_container(&orig_, &iter_);
   }


   template<typename T>
   void operator()(const T& t);


   DBusMessageIter& orig_;
   DBusMessageIter iter_;
};


struct TupleDeserializer // : noncopable
{
   inline
   TupleDeserializer(DBusMessageIter& iter, bool flattened = false)
    : orig_(iter)
    , use_(flattened?&orig_:&iter_)
    , flattened_(flattened)
   {
      if (!flattened)
         simppl_dbus_message_iter_recurse(&orig_, &iter_, DBUS_TYPE_STRUCT);
   }

   ~TupleDeserializer()
   {
      if (!flattened_)
         dbus_message_iter_next(&orig_);
   }

   template<typename T>
   void operator()(T& t);

   
   DBusMessageIter& orig_;
   DBusMessageIter iter_;
   
   DBusMessageIter* use_;
   
   bool flattened_;
};


}   // namespace detail

   
template<typename... T>
struct Codec<std::tuple<T...>>
{
   static 
   void encode(DBusMessageIter& iter, const std::tuple<T...>& t)
   {
      detail::TupleSerializer ts(iter);
      std_tuple_for_each(t, std::ref(ts));
   }
   
   
   static 
   void decode(DBusMessageIter& iter, std::tuple<T...>& t)
   {
      detail::TupleDeserializer tds(iter);
      std_tuple_for_each(t, std::ref(tds));
   }
   
   
   static
   void decode_flattened(DBusMessageIter& iter, std::tuple<T...>& t)
   {
      detail::TupleDeserializer tds(iter, true);
      std_tuple_for_each(t, std::ref(tds));
   }
   
   
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
         Codec<U>::make_type_signature(os_);
      }

      std::ostream& os_;
   };


   static
   std::ostream& make_type_signature(std::ostream& os)
   {
      os << DBUS_STRUCT_BEGIN_CHAR_AS_STRING;
      
      std::tuple<T...>* t = nullptr;
      std_tuple_for_each(*t, helper(os));
      
      os << DBUS_STRUCT_END_CHAR_AS_STRING;

      return os;
   }
};


template<typename T>
inline
void detail::TupleDeserializer::operator()(T& t)
{
   decode(*use_, t);
}


template<typename T>
inline
void detail::TupleSerializer::operator()(const T& t)   // seems to be already a reference so no copy is done
{
   encode(iter_, t);
}

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_TUPLE_H
