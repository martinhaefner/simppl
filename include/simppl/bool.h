#ifndef SIMPPL_DBUS_BOOL_H
#define SIMPPL_DBUS_BOOL_H


#include "simppl/serialization.h"


namespace simppl
{
   
namespace dbus
{

   
struct BoolCodec : composite_signature<signature_chars<DBUS_TYPE_BOOLEAN>>
{
   static 
   void encode(DBusMessageIter& iter, bool b);

   static 
   void decode(DBusMessageIter& iter, bool& t);
};
   

template<>
struct Codec<bool> : BoolCodec {};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_BOOL_H
