#ifndef SIMPPL_DBUS_STRING_H
#define SIMPPL_DBUS_STRING_H


#include <string>

#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{
 
 
struct StringCodec
{
   static 
   void encode(DBusMessageIter& s, const std::string& str);
   
   static 
   void decode(DBusMessageIter& s, std::string& str);
   
   static 
   void encode(DBusMessageIter& s, const char* str);
   
   static 
   void decode(DBusMessageIter& s, char*& str);
   
   static
   std::ostream& make_type_signature(std::ostream& os);
};

   
template<>
struct Codec<std::string> : public StringCodec {};

template<>
struct Codec<char*> : public StringCodec {};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_STRING_H
