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
{
   static 
   void encode(DBusMessageIter& iter, const std::pair<KeyT, ValueT>& p)
   {
      DBusMessageIter item_iterator;
      dbus_message_iter_open_container(&iter, DBUS_TYPE_DICT_ENTRY, nullptr, &item_iterator);

      Codec<KeyT>::encode(item_iterator, p.first);
      Codec<ValueT>::encode(item_iterator, p.second);
      
      dbus_message_iter_close_container(&iter, &item_iterator);
   }
   
   
   static 
   void decode(DBusMessageIter& iter, std::pair<KeyT, ValueT>& p)
   {
      DBusMessageIter item_iterator;
      simppl_dbus_message_iter_recurse(&iter, &item_iterator, DBUS_TYPE_DICT_ENTRY);

      Codec<KeyT>::decode(item_iterator, p.first);
      Codec<ValueT>::decode(item_iterator, p.second);
      
      // advance to next element
      dbus_message_iter_next(&iter);
   }
   
   
   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      os << DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING;
      
      Codec<KeyT>::make_type_signature(os);
      Codec<ValueT>::make_type_signature(os);
      
      return os << DBUS_DICT_ENTRY_END_CHAR_AS_STRING;
   }
};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_PAIR_H
