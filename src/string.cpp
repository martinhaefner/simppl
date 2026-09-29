#include "simppl/string.h"

#include <cstring>
#include <cassert>


namespace simppl
{
   
namespace dbus
{
   

/*static*/
void StringCodec::encode(Encoder& e, const std::string& str)
{
   const char* c_str = str.c_str();
   e.append_basic(DBUS_TYPE_STRING, &c_str);
}


/*static*/ 
void StringCodec::decode(Decoder& d, std::string& str)
{   
   char* c_str = nullptr;
   d.get_basic(&c_str, DBUS_TYPE_STRING);
   
   if (c_str)
   {
      str.assign(c_str);
   }
   else
      str.clear();
}


/*static*/ 
void StringCodec::encode(Encoder& e, const char* str)
{
   e.append_basic(DBUS_TYPE_STRING, &str);
}


/*static*/ 
void StringCodec::decode(Decoder& d, char*& str)
{   
   assert(str == nullptr);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free

   char* c_str = nullptr;
   d.get_basic(&c_str, DBUS_TYPE_STRING);
   
   // FIXME trouble with allocated memory in case of exception
   if (c_str)
   {
      str = (char*)new char[strlen(c_str)+1];
      strcpy(str, c_str);
   }
}


}   // namespace dbus

}   // namespace simppl
