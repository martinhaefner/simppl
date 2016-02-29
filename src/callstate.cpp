#include "simppl/callstate.h"

#include <dbus/dbus.h>

#include <cstring>

#include "simppl/serialization.h"


namespace simppl
{
   
namespace dbus
{


CallState::CallState(DBusMessage& msg)
 : ex_()
 , sequence_nr_(INVALID_SEQUENCE_NR)
{
   if (dbus_message_get_type(&msg) == DBUS_MESSAGE_TYPE_ERROR)
   {
      if (!strcmp(dbus_message_get_error_name(&msg), DBUS_ERROR_FAILED))
      {
         detail::Deserializer d(&msg);
         std::string text;
         d >> text;
         
         char* end;
         int error = strtol(text.c_str(), &end, 10);
         
         ex_.reset(new RuntimeError(error, end+1, dbus_message_get_reply_serial(&msg)));
      }
      else
         ex_.reset(new TransportError(EIO, dbus_message_get_reply_serial(&msg)));
   }
   else
      sequence_nr_ = dbus_message_get_reply_serial(&msg);
}


void CallState::throw_exception() const
{
   assert(ex_);
   ex_->_throw();
}


}   // namespace dbus

}   // namespace simppl
