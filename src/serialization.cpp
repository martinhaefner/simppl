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


detail::Serializer& detail::Serializer::write(const char* str)
{
   char* c_str = const_cast<char*>(str);
   dbus_message_iter_append_basic(iter_, DBUS_TYPE_STRING, &c_str);
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
   assert(str == 0);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free

   char* c_str = 0;
   dbus_message_iter_get_basic(iter_, &c_str);
   dbus_message_iter_next(iter_);

   if (c_str)
   {
      str = allocate(strlen(c_str)+1);
      strcpy(str, c_str);
   }

   return *this;
}


detail::Deserializer& detail::Deserializer::read(std::string& str)
{
   char* c_str = 0;
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


detail::Deserializer& detail::Deserializer::read(ObjectPath& p)
{
   char* c_str = 0;
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
