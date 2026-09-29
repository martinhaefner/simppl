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
   void encode(Encoder& e, const std::string& str);
   
   static 
   void decode(Decoder& d, std::string& str);
   
   static 
   void encode(Encoder& e, const char* str);
   
   static 
   void decode(Decoder& d, char*& str);
};

   
template<>
struct Codec<std::string> : public StringCodec {};

template<>
struct Codec<char*> : public StringCodec {};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_STRING_H
