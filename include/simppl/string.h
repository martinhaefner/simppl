#ifndef SIMPPL_DBUS_STRING_H
#define SIMPPL_DBUS_STRING_H


#include <string>

#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{
 
 
struct StringCodec : composite_signature<signature_chars<DBUS_TYPE_STRING>>
{
   static 
   void encode(DBusMessageIter& s, const std::string& str);
   
   static 
   void decode(DBusMessageIter& s, std::string& str);
   
   static 
   void encode(DBusMessageIter& s, const char* str);
   
   static 
   void decode(DBusMessageIter& s, char*& str);
};

   
template<>
struct Codec<std::string> : public StringCodec {};

template<>
struct Codec<char*> : public StringCodec {};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_STRING_H
