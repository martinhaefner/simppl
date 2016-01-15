#include "simppl/detail/serialization.h"


namespace simppl
{

namespace ipc
{

detail::Serializer::Serializer(DBusMessage* msg)
 : parent_(nullptr)
{
    dbus_message_iter_init_append(msg, &iter_);
}


detail::Serializer::Serializer(DBusMessageIter* iter)
 : parent_(iter)
{
    dbus_message_iter_open_container(parent_, DBUS_TYPE_STRUCT, nullptr, &iter_);
}


detail::Serializer::~Serializer()
{
    if (parent_)
        dbus_message_iter_close_container(parent_, &iter_);
}


detail::Serializer& detail::Serializer::write(const std::string& str)
{
   char* c_str = const_cast<char*>(str.c_str());
   dbus_message_iter_append_basic(&iter_, DBUS_TYPE_STRING, &c_str);
   return *this;
}


// ----------------------------------------------------------------------------


detail::Deserializer::Deserializer(DBusMessage* msg)
 : parent_(0)
{
    dbus_message_iter_init(msg, &iter_);
}


detail::Deserializer::Deserializer(DBusMessageIter* iter)
 : parent_(iter)
{
    dbus_message_iter_open_container(parent_, DBUS_TYPE_STRUCT, nullptr, &iter_);
}


detail::Deserializer::~Deserializer()
{
    if (parent_)
        dbus_message_iter_close_container(parent_, &iter_);
}


detail::Deserializer& detail::Deserializer::read(char*& str)
{
   assert(str == 0);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free

   char* c_str = 0;
   dbus_message_iter_get_basic(&iter_, &c_str);

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
   dbus_message_iter_get_basic(&iter_, &c_str);

   if (c_str)
   {
      str.assign(c_str);
   }
   else
      str.clear();

   return *this;
}

}   // namespace ipc

}   // namespace simppl
