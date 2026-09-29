#ifndef SIMPPL_DBUS_TUPLE_H
#define SIMPPL_DBUS_TUPLE_H


#include <tuple>

#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{

template<typename... T>
struct Codec<std::tuple<T...>>
 : composite_signature<signature_chars<DBUS_STRUCT_BEGIN_CHAR>, Codec<T>..., signature_chars<DBUS_STRUCT_END_CHAR>>
{
   static 
   void encode(Encoder& e, const std::tuple<T...>& t)
   {
      Encoder members = e.open_container(DBUS_TYPE_STRUCT);

      std::apply([&members](const T&... m){ (detail::encode_one<T>(members, m), ...); }, t);
   }
   
   
   static 
   void decode(Decoder& d, std::tuple<T...>& t)
   {
      Decoder members = d.recurse(DBUS_TYPE_STRUCT);
      decode_flattened(members, t);

      // advance to next element
      d.next();
   }
   
   
   /// decode the members without the surrounding struct, e.g. function arguments
   static
   void decode_flattened(Decoder& d, std::tuple<T...>& t)
   {
      std::apply([&d](T&... m){ (detail::decode_one<T>(d, m), ...); }, t);
   }
};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_TUPLE_H
