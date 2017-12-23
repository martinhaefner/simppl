#ifndef __SIMPPL_DBUS_BOOL_H__
#define __SIMPPL_DBUS_BOOL_H__


#include "simppl/serialization.h"


namespace simppl
{
   
namespace dbus
{


// forward decl
template<typename T>
struct Codec;
   
   
template<>
struct Codec<bool>
{
   static inline
   void encode(DBusMessageIter& iter, bool b)
   {
      dbus_bool_t _b = b;
      dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &_b);
   }


   static inline 
   void decode(DBusMessageIter& iter, bool& t)
   {
      dbus_bool_t b;
      dbus_message_iter_get_basic(&iter, &b);
      dbus_message_iter_next(&iter);

      t = b;
   }
   
   
   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return os << DBUS_TYPE_BOOLEAN_AS_STRING;
   }
};


}   // namespace dbus

}   // namespace simppl


#endif   // __SIMPPL_DBUS_BOOL_H__
