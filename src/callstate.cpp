#include "simppl/callstate.h"


simppl::dbus::CallState::CallState(DBusMessage& msg)
 : ex_()
 , serial_(SIMPPL_INVALID_SERIAL)
{
   if (dbus_message_get_type(&msg) == DBUS_MESSAGE_TYPE_ERROR)
   {	  
	  ex_.reset(new Error);
	  detail::ErrorFactory<Error>::init(*ex_, msg);
   }   
   else
      serial_ = dbus_message_get_reply_serial(&msg);
}
