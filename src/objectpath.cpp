#include "simppl/objectpath.h"


namespace simppl
{
   
namespace dbus
{
   

/*static*/ 
void ObjectPathCodec::encode(DBusMessageIter& iter, const ObjectPath& p)
{
   char* c_str = const_cast<char*>(p.path.c_str());
   dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &c_str);
}


/*static*/ 
void ObjectPathCodec::decode(DBusMessageIter& iter, ObjectPath& p)
{   
   char* c_str = nullptr;  
   simppl_dbus_message_iter_get_basic(&iter, &c_str, DBUS_TYPE_OBJECT_PATH);
   
   if (c_str)
   {
      p.path.assign(c_str);
   }
   else
      p.path.clear();
}


/*static*/
std::ostream& ObjectPathCodec::make_type_signature(std::ostream& os)
{
   return os << DBUS_TYPE_OBJECT_PATH_AS_STRING;
}


}   // namespace dbus

}   // namespace simppl

