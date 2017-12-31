#ifndef SIMPPL_DBUS_BOOL_H
#define SIMPPL_DBUS_BOOL_H


#include "simppl/serialization.h"


namespace simppl
{
   
namespace dbus
{

   
struct BoolCodec
{
   static 
   void encode(DBusMessageIter& iter, bool b);

   static 
   void decode(DBusMessageIter& iter, bool& t);
   
   static
   std::ostream& make_type_signature(std::ostream& os);
};
   

template<>
struct Codec<bool> : BoolCodec {};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_BOOL_H
