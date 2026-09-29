#include "simppl/wstring.h"

#include <cstring>
#include <cassert>


namespace simppl
{

namespace dbus
{


/*static*/
void WStringCodec::encode(Encoder& e, const std::wstring& str)
{
   static_assert(sizeof(uint32_t) == sizeof(wchar_t), "data types mapping does not match");

   Encoder array = e.open_container(DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32_AS_STRING);

   for (auto& t : str) {
      Codec<uint32_t>::encode(array, (uint32_t)t);
   }
}


/*static*/
void WStringCodec::decode(Decoder& d, std::wstring& str)
{
   str.clear();

   Decoder array = d.recurse(DBUS_TYPE_ARRAY);

   int count =
#if DBUS_MAJOR_VERSION == 1 && DBUS_MINOR_VERSION < 9
       dbus_message_iter_get_array_len(&array.native()) / sizeof(uint32_t);
#else
       dbus_message_iter_get_element_count(&d.native());
#endif
   if (count > 0)
      str.reserve(count);

   while(!array.at_end())
   {
      uint32_t t;
      Codec<uint32_t>::decode(array, t);
      str.push_back((wchar_t)t);
   }

   // advance to next element
   d.next();
}


/*static*/
void WStringCodec::encode(Encoder& e, const wchar_t* str)
{
   Encoder array = e.open_container(DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32_AS_STRING);

   while(str && *str) {
      Codec<uint32_t>::encode(array, (uint32_t)*str++);
   }
}


/*static*/
void WStringCodec::decode(Decoder& d, wchar_t*& str)
{
   wchar_t* c_str = nullptr;

   //assert(str == nullptr);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free

   Decoder array = d.recurse(DBUS_TYPE_ARRAY);

   int count =
#if DBUS_MAJOR_VERSION == 1 && DBUS_MINOR_VERSION < 9
       dbus_message_iter_get_array_len(&array.native()) / sizeof(uint32_t);
#else
       dbus_message_iter_get_element_count(&d.native());
#endif
   if (count > 0)
   {
      c_str = new wchar_t[count+1];
      c_str[count] = 0;

      int i = 0;
      while(!array.at_end())
      {
         uint32_t t;
         Codec<uint32_t>::decode(array, t);
         c_str[i++] = (wchar_t)t;
      }

      str = c_str;
   }
   else
      str = nullptr;

   // advance to next element
   d.next();
}


}   // namespace dbus

}   // namespace simppl
