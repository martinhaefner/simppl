#include "simppl/bool.h"


namespace simppl
{
   
namespace dbus
{


/*static*/
void BoolCodec::encode(DBusMessageIter& iter, bool b)
{
   dbus_bool_t _b = b;
   dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &_b);
}


/*static*/  
void BoolCodec::decode(DBusMessageIter& iter, bool& t)
{
   dbus_bool_t b;
   simppl_dbus_message_iter_get_basic(&iter, &b, DBUS_TYPE_BOOLEAN);
   
   t = b;
}


/*static*/ 
std::ostream& BoolCodec::make_type_signature(std::ostream& os)
{
   return os << DBUS_TYPE_BOOLEAN_AS_STRING;
}


}   // namespace dbus

}   // namespace simppl

