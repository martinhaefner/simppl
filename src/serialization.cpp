//#include "simppl/detail/serialization.h"


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

#if 0
detail::Serializer& detail::Serializer::write(const std::string& str)
{
   return *this;
}
#endif


// FIXME maybe we don't need it at all. Write a test to verify...
detail::Serializer& detail::Serializer::write_ptr(const char* str)
{
   return *this;
}


detail::Serializer& detail::Serializer::write(const std::wstring& str)
{
   
   return *this;
}


detail::Serializer& detail::Serializer::write_ptr(const wchar_t* str)
{
   
   return *this;
}


detail::Serializer& detail::Serializer::write(const ObjectPath& p)
{
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
   
   return *this;
}

#if 0
detail::Deserializer& detail::Deserializer::read(std::string& str)
{
   
   return *this;
}
#endif

detail::Deserializer& detail::Deserializer::read(wchar_t*& str)
{
 
   return *this;
}


detail::Deserializer& detail::Deserializer::read(std::wstring& str)
{
   
   return *this;
}


detail::Deserializer& detail::Deserializer::read(ObjectPath& p)
{
   
   return *this;
}

}   // namespace dbus

}   // namespace simppl
