#ifndef SIMPPL_DBUS_VECTOR_H
#define SIMPPL_DBUS_VECTOR_H


#include <vector>

#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{
    
   
template<typename T>
struct Codec<std::vector<T>>
{
   static 
   void encode(DBusMessageIter& s, const std::vector<T>& v)
   {
      DBusMessageIter iter;
      
      std::ostringstream buf;
      Codec<T>::make_type_signature(buf);

      dbus_message_iter_open_container(&s, DBUS_TYPE_ARRAY, buf.str().c_str(), &iter);

      for (auto& t : v) 
      {
         Codec<T>::encode(iter, t);
      }

      dbus_message_iter_close_container(&s, &iter);
   }
   
   
   static 
   void decode(DBusMessageIter& s, std::vector<T>& v)
   {
      v.clear();

      DBusMessageIter iter;
      simppl_dbus_message_iter_recurse(&s, &iter, DBUS_TYPE_ARRAY);

      while(dbus_message_iter_get_arg_type(&iter) != 0)
      {
         T t;
         Codec<T>::decode(iter, t);
         v.push_back(t);
      }

      // advance to next element
      dbus_message_iter_next(&s);
   }
   
   
   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return Codec<T>::make_type_signature(os << DBUS_TYPE_ARRAY_AS_STRING);
   }

};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_VECTOR_H
