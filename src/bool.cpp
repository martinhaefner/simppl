#include "simppl/bool.h"


namespace simppl
{
   
namespace dbus
{


/*static*/
void BoolCodec::encode(Encoder& e, bool b)
{
   dbus_bool_t _b = b;
   e.append_basic(DBUS_TYPE_BOOLEAN, &_b);
}


/*static*/  
void BoolCodec::decode(Decoder& d, bool& t)
{
   dbus_bool_t b;
   d.get_basic(&b, DBUS_TYPE_BOOLEAN);
   
   t = b;
}


}   // namespace dbus

}   // namespace simppl

