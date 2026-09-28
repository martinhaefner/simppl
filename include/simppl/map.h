#ifndef SIMPPL_DBUS_MAP_H
#define SIMPPL_DBUS_MAP_H


#include <map>

#include "simppl/pair.h"


namespace simppl
{

namespace dbus
{

   
template<typename KeyT, typename ValueT>
struct Codec<std::map<KeyT, ValueT>>
 : composite_signature<signature_chars<DBUS_TYPE_ARRAY>, Codec<std::pair<typename std::decay<KeyT>::type, ValueT>>>
{
   static 
   void encode(DBusMessageIter& iter, const std::map<KeyT, ValueT>& m)
   {
      DBusMessageIter _iter;
      dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, signature_of<std::pair<KeyT, ValueT>>(), &_iter);

      for (auto& e : m) {
         Codec<std::pair<KeyT, ValueT>>::encode(_iter, e);
      }

      dbus_message_iter_close_container(&iter, &_iter);
   }
   
   
   static 
   void decode(DBusMessageIter& iter, std::map<KeyT, ValueT>& m)
   {
      m.clear();
      
      DBusMessageIter _iter;
      simppl_dbus_message_iter_recurse(&iter, &_iter, DBUS_TYPE_ARRAY);

      while(dbus_message_iter_get_arg_type(&_iter) != 0)
      {
         std::pair<KeyT, ValueT> p;
         Codec<decltype(p)>::decode(_iter, p);

         m.insert(p);
      }

      // advance to next element
      dbus_message_iter_next(&iter);
   }
};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_MAP_H
