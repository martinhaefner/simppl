#include "simppl/detail/serialization.h"


namespace simppl
{

namespace dbus
{

detail::Serializer::Serializer(DBusMessage* msg)
 : iter_(&private_iter_)
{
    dbus_message_iter_init_append(msg, &private_iter_);
}


detail::Serializer::Serializer(DBusMessageIter* iter)
 : iter_(iter)
{
    // NOOP
}


detail::Serializer& detail::Serializer::write(const std::string& str)
{
   char* c_str = const_cast<char*>(str.c_str());
   dbus_message_iter_append_basic(iter_, DBUS_TYPE_STRING, &c_str);
   return *this;
}


// FIXME maybe we don't need it at all. Write a test to verify...
detail::Serializer& detail::Serializer::write_ptr(const char* str)
{
   char* c_str = const_cast<char*>(str);
   dbus_message_iter_append_basic(iter_, DBUS_TYPE_STRING, &c_str);
   return *this;
}


detail::Serializer& detail::Serializer::write(const std::wstring& str)
{
   static_assert(sizeof(uint32_t) == sizeof(wchar_t), "data types mapping does not match");
   
   DBusMessageIter iter;
   
   dbus_message_iter_open_container(iter_, DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32_AS_STRING, &iter);

   Serializer s(&iter);
   for (auto& t : str) {
      s.write((uint32_t)t);
   }

   dbus_message_iter_close_container(iter_, &iter);

   return *this;
}


detail::Serializer& detail::Serializer::write_ptr(const wchar_t* str)
{
   DBusMessageIter iter;
   
   dbus_message_iter_open_container(iter_, DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32_AS_STRING, &iter);

   Serializer s(&iter);
   
   while(str && *str) {
      s.write((uint32_t)*str++);
   }

   dbus_message_iter_close_container(iter_, &iter);

   return *this;
}


detail::Serializer& detail::Serializer::write(const ObjectPath& p)
{
   char* c_str = const_cast<char*>(p.path.c_str());
   dbus_message_iter_append_basic(iter_, DBUS_TYPE_OBJECT_PATH, &c_str);
   return *this;
}


// ----------------------------------------------------------------------------


detail::Deserializer::Deserializer(DBusMessage* msg)
 : iter_(&private_iter_)
{
    dbus_message_iter_init(msg, &private_iter_);
}


detail::Deserializer::Deserializer(DBusMessageIter* iter)
 : iter_(iter)
{
    // NOOP - FIXME may call recurse in-here
}


detail::Deserializer& detail::Deserializer::read(char*& str)
{
   assert(str == nullptr);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free

   char* c_str = nullptr;
   dbus_message_iter_get_basic(iter_, &c_str);
   dbus_message_iter_next(iter_);

   if (c_str)
   {
      str = (char*)allocate(strlen(c_str)+1);
      strcpy(str, c_str);
   }

   return *this;
}


detail::Deserializer& detail::Deserializer::read(std::string& str)
{
   char* c_str = nullptr;
   dbus_message_iter_get_basic(iter_, &c_str);
   dbus_message_iter_next(iter_);

   if (c_str)
   {
      str.assign(c_str);
   }
   else
      str.clear();

   return *this;
}


detail::Deserializer& detail::Deserializer::read(wchar_t*& str)
{
   wchar_t* c_str = nullptr;
   
   //assert(str == nullptr);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free

   DBusMessageIter iter;
   dbus_message_iter_recurse(iter_, &iter);
   
   int count = 
#if DBUS_MAJOR_VERSION == 1 && DBUS_MINOR_VERSION < 7
       dbus_message_iter_get_array_len(&iter) / sizeof(uint32_t);
#else
       dbus_message_iter_get_element_count(&iter);
#endif
   if (count > 0)
   {
      c_str = (wchar_t*)allocate((count+1) * sizeof(wchar_t));
      c_str[count] = 0;
   
      Deserializer s(&iter);

      int i = 0;
      while(dbus_message_iter_get_arg_type(&iter) != 0)
      {
         uint32_t t;
         s.read(t);
         c_str[i++] = (wchar_t)t;
      }
      
      str = c_str;
   }
   else
      str = nullptr;
   
   // advance to next element
   dbus_message_iter_next(iter_);

   return *this;
}


detail::Deserializer& detail::Deserializer::read(std::wstring& str)
{
   str.clear();

   DBusMessageIter iter;
   dbus_message_iter_recurse(iter_, &iter);
   
   int count = 
#if DBUS_MAJOR_VERSION == 1 && DBUS_MINOR_VERSION < 7
       dbus_message_iter_get_array_len(&iter) / sizeof(uint32_t);
#else
       dbus_message_iter_get_element_count(&iter);
#endif
   if (count > 0)
      str.reserve(count);
   
   Deserializer s(&iter);

   while(dbus_message_iter_get_arg_type(&iter) != 0)
   {
      uint32_t t;
      s.read(t);
      str.push_back((wchar_t)t);
   }

   // advance to next element
   dbus_message_iter_next(iter_);

   return *this;
}


detail::Deserializer& detail::Deserializer::read(ObjectPath& p)
{
   char* c_str = nullptr;
   dbus_message_iter_get_basic(iter_, &c_str);
   dbus_message_iter_next(iter_);

   if (c_str)
   {
      p.path.assign(c_str);
   }
   else
      p.path.clear();

   return *this;
}

}   // namespace dbus

}   // namespace simppl
