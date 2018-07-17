#include "simppl/wstring.h"

#include <cstring>
#include <cassert>


namespace simppl
{

namespace dbus
{


/*static*/
void WStringCodec::encode(DBusMessageIter& iter, const std::wstring& str)
{
   static_assert(sizeof(uint32_t) == sizeof(wchar_t), "data types mapping does not match");

   DBusMessageIter _iter;

   dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32_AS_STRING, &_iter);

   for (auto& t : str) {
      Codec<uint32_t>::encode(_iter, (uint32_t)t);
   }

   dbus_message_iter_close_container(&iter, &_iter);
}


/*static*/
void WStringCodec::decode(DBusMessageIter& iter, std::wstring& str)
{
   str.clear();

   DBusMessageIter _iter;
   simppl_dbus_message_iter_recurse(&iter, &_iter, DBUS_TYPE_ARRAY);

   int count =
#if DBUS_MAJOR_VERSION == 1 && DBUS_MINOR_VERSION < 9
       dbus_message_iter_get_array_len(&_iter) / sizeof(uint32_t);
#else
       dbus_message_iter_get_element_count(&iter);
#endif
   if (count > 0)
      str.reserve(count);

   while(dbus_message_iter_get_arg_type(&_iter) != 0)
   {
      uint32_t t;
      Codec<uint32_t>::decode(_iter, t);
      str.push_back((wchar_t)t);
   }

   // advance to next element
   dbus_message_iter_next(&iter);
}


/*static*/
void WStringCodec::encode(DBusMessageIter& iter, const wchar_t* str)
{
   DBusMessageIter _iter;

   dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32_AS_STRING, &_iter);

   while(str && *str) {
      Codec<uint32_t>::encode(_iter, (uint32_t)*str++);
   }

   dbus_message_iter_close_container(&iter, &_iter);
}


/*static*/
void WStringCodec::decode(DBusMessageIter& iter, wchar_t*& str)
{
   wchar_t* c_str = nullptr;

   //assert(str == nullptr);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free

   DBusMessageIter _iter;
   simppl_dbus_message_iter_recurse(&iter, &_iter, DBUS_TYPE_ARRAY);

   int count =
#if DBUS_MAJOR_VERSION == 1 && DBUS_MINOR_VERSION < 9
       dbus_message_iter_get_array_len(&_iter) / sizeof(uint32_t);
#else
       dbus_message_iter_get_element_count(&iter);
#endif
   if (count > 0)
   {
      c_str = new wchar_t[count+1];
      c_str[count] = 0;

      int i = 0;
      while(dbus_message_iter_get_arg_type(&_iter) != 0)
      {
         uint32_t t;
         Codec<uint32_t>::decode(_iter, t);
         c_str[i++] = (wchar_t)t;
      }

      str = c_str;
   }
   else
      str = nullptr;

   // advance to next element
   dbus_message_iter_next(&iter);
}


/*static*/
std::ostream& WStringCodec::make_type_signature(std::ostream& os)
{
   return Codec<uint32_t>::make_type_signature(os << DBUS_TYPE_ARRAY_AS_STRING);
}


}   // namespace dbus

}   // namespace simppl
