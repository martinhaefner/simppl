#include "simppl/callstate.h"

#include <dbus/dbus.h>

#include <cstring>
#include <cassert>


namespace simppl
{

namespace dbus
{


CallState::CallState(DBusMessage& msg)
 : ex_()
 , serial_(SIMPPL_INVALID_SERIAL)
{
   if (dbus_message_get_type(&msg) == DBUS_MESSAGE_TYPE_ERROR)
   {
      ex_ = Error::from_message(msg);
   }
   else
      serial_ = dbus_message_get_reply_serial(&msg);
}


void CallState::throw_exception() const
{
   assert(ex_);
   ex_->_throw();
}


}   // namespace dbus

}   // namespace simppl
