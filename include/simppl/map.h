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
   void encode(Encoder& e, const std::map<KeyT, ValueT>& m)
   {
      Encoder array = e.open_container(DBUS_TYPE_ARRAY, signature_of<std::pair<KeyT, ValueT>>());

      for (auto& entry : m) {
         Codec<std::pair<KeyT, ValueT>>::encode(array, entry);
      }
   }
   
   
   static 
   void decode(Decoder& d, std::map<KeyT, ValueT>& m)
   {
      m.clear();
      
      Decoder array = d.recurse(DBUS_TYPE_ARRAY);

      while(!array.at_end())
      {
         std::pair<KeyT, ValueT> p;
         Codec<decltype(p)>::decode(array, p);

         m.insert(p);
      }

      // advance to next element
      d.next();
   }
};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_MAP_H
