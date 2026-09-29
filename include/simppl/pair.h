#ifndef SIMPPL_DBUS_PAIR_H
#define SIMPPL_DBUS_PAIR_H


#include <map>

#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{
 
   
template<typename KeyT, typename ValueT>
struct Codec<std::pair<KeyT, ValueT>>
 : composite_signature<signature_chars<DBUS_DICT_ENTRY_BEGIN_CHAR>, Codec<KeyT>, Codec<ValueT>, signature_chars<DBUS_DICT_ENTRY_END_CHAR>>
{
   static 
   void encode(Encoder& e, const std::pair<KeyT, ValueT>& p)
   {
      Encoder entry = e.open_container(DBUS_TYPE_DICT_ENTRY);

      detail::encode_one<KeyT>(entry, p.first);
      detail::encode_one<ValueT>(entry, p.second);
   }
   
   
   static 
   void decode(Decoder& d, std::pair<KeyT, ValueT>& p)
   {
      Decoder entry = d.recurse(DBUS_TYPE_DICT_ENTRY);

      detail::decode_one<KeyT>(entry, p.first);
      detail::decode_one<ValueT>(entry, p.second);
      
      // advance to next element
      d.next();
   }
};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_PAIR_H
