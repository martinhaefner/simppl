#include "simppl/string.h"

#include <cstring>
#include <cassert>


namespace simppl
{
   
namespace dbus
{
   

/*static*/
void StringCodec::encode(DBusMessageIter& iter, const std::string& str)
{
   char* c_str = const_cast<char*>(str.c_str());
   dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &c_str);
}


/*static*/ 
void StringCodec::decode(DBusMessageIter& iter, std::string& str)
{   
   char* c_str = nullptr;
   simppl_dbus_message_iter_get_basic(&iter, &c_str, DBUS_TYPE_STRING);
   
   if (c_str)
   {
      str.assign(c_str);
   }
   else
      str.clear();
}


/*static*/ 
void StringCodec::encode(DBusMessageIter& iter, const char* str)
{
   char* c_str = const_cast<char*>(str);
   dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &c_str);
}


/*static*/ 
void StringCodec::decode(DBusMessageIter& iter, char*& str)
{   
   assert(str == nullptr);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free

   char* c_str = nullptr;
   simppl_dbus_message_iter_get_basic(&iter, &c_str, DBUS_TYPE_STRING);
   
   // FIXME trouble with allocated memory in case of exception
   if (c_str)
   {
      str = (char*)new char[strlen(c_str)+1];
      strcpy(str, c_str);
   }
}


/*static*/
std::ostream& StringCodec::make_type_signature(std::ostream& os)
{
   return os << DBUS_TYPE_STRING_AS_STRING;
}


}   // namespace dbus

}   // namespace simppl
