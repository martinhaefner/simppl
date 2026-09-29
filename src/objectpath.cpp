#include "simppl/objectpath.h"


namespace simppl
{
   
namespace dbus
{
   

/*static*/ 
void ObjectPathCodec::encode(Encoder& e, const ObjectPath& p)
{
   const char* c_str = p.path.c_str();
   e.append_basic(DBUS_TYPE_OBJECT_PATH, &c_str);
}


/*static*/ 
void ObjectPathCodec::decode(Decoder& d, ObjectPath& p)
{   
   char* c_str = nullptr;  
   d.get_basic(&c_str, DBUS_TYPE_OBJECT_PATH);
   
   if (c_str)
   {
      p.path.assign(c_str);
   }
   else
      p.path.clear();
}


}   // namespace dbus

}   // namespace simppl

