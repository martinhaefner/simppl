#ifndef __SIMPPL_DBUS_MAP_H__
#define __SIMPPL_DBUS_MAP_H__


#include <map>

#include "simppl/pair.h"


namespace simppl
{

namespace dbus
{

namespace detail
{
 
   
template<typename KeyT, typename ValueT>
struct Codec<std::map<KeyT, ValueT>>
{
   static 
   void encode(Serializer& s, const std::map<KeyT, ValueT>& m)
   {
      std::ostringstream buf;
      make_type_signature<std::pair<KeyT, ValueT>>::eval(buf);

      DBusMessageIter iter;
      dbus_message_iter_open_container(s.iter_, DBUS_TYPE_ARRAY, buf.str().c_str(), &iter);

      Serializer s1(&iter);

      for (auto& e : m) {
         Codec<decltype(e)>::encode(s1, e);
      }

      dbus_message_iter_close_container(s.iter_, &iter);
   }
   
   
   static 
   void decode(Deserializer& s, std::map<KeyT, ValueT>& m)
   {
      m.clear();

      DBusMessageIter iter;
      dbus_message_iter_recurse(s.iter_, &iter);

      Deserializer ds(&iter);

      while(dbus_message_iter_get_arg_type(&iter) != 0)
      {
         std::pair<KeyT, ValueT> p;
         Codec<decltype(p)>::decode(ds, p);

         m.insert(p);
      }

      // advance to next element
      dbus_message_iter_next(s.iter_);

   }
};


template<typename KeyT, typename ValueT>
struct make_type_signature<std::map<KeyT, ValueT>>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return make_type_signature<std::pair<typename std::decay<KeyT>::type, ValueT>>::eval(os << DBUS_TYPE_ARRAY_AS_STRING);
   }
};

   
}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // __SIMPPL_DBUS_MAP_H__
