#ifndef __SIMPPL_DBUS_STRING_H__
#define __SIMPPL_DBUS_STRING_H__


#include <string>


namespace simppl
{

namespace dbus
{

namespace detail
{
 
   
template<>
struct Codec<std::string>
{
   static 
   void encode(Serializer& s, const std::string& str)
   {
      // FIXME
   }
   
   
   static 
   void decode(Deserializer& s, std::string& str)
   {
      // FIXME
   }
};


 Serializer& write_ptr(const char* str);

   Serializer& write(const std::wstring& str);
   Serializer& write_ptr(const wchar_t* str);
  
  
  Deserializer& read(char*& str);
   Deserializer& read(wchar_t*& str);
   Deserializer& read(std::wstring& str);
   

template<>
struct make_type_signature<std::string>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return os << DBUS_TYPE_STRING_AS_STRING;
   }
};


template<>
struct make_type_signature<std::wstring>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return make_type_signature<uint32_t>::eval(os << DBUS_TYPE_ARRAY_AS_STRING);
   }
};


template<>
struct make_type_signature<wchar_t*>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return make_type_signature<uint32_t>::eval(os << DBUS_TYPE_ARRAY_AS_STRING);
   }
};


   
}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // __SIMPPL_DBUS_STRING_H__
