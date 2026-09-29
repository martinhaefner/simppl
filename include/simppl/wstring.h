#ifndef SIMPPL_DBUS_WSTRING_H
#define SIMPPL_DBUS_WSTRING_H


#include <string>

#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{
 
struct WStringCodec : composite_signature<signature_chars<DBUS_TYPE_ARRAY>, Codec<uint32_t>>
{
   static 
   void encode(Encoder& e, const std::wstring& str);
   
   static 
   void decode(Decoder& d, std::wstring& str);
   
   static 
   void encode(Encoder& e, const wchar_t* str);
   
   static 
   void decode(Decoder& d, wchar_t*& str);
};

   
template<>
struct Codec<std::wstring> : public WStringCodec {};


template<>
struct Codec<wchar_t*> : public WStringCodec {};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_STRING_H
