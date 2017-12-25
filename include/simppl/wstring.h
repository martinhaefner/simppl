#ifndef SIMPPL_DBUS_WSTRING_H
#define SIMPPL_DBUS_WSTRING_H


#include <string>

#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{
 
struct WStringCodec
{
   static 
   void encode(DBusMessageIter& s, const std::wstring& str);
   
   static 
   void decode(DBusMessageIter& s, std::wstring& str);
   
   static 
   void encode(DBusMessageIter& s, const wchar_t* str);
   
   static 
   void decode(DBusMessageIter& s, wchar_t*& str);
   
   static
   std::ostream& make_type_signature(std::ostream& os);
};

   
template<>
struct Codec<std::wstring> : public WStringCodec {};


template<>
struct Codec<wchar_t*> : public WStringCodec {};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_STRING_H
