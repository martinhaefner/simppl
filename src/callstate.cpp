#include "simppl/callstate.h"


/*static*/
std::unique_ptr<simppl::dbus::Error> simppl::dbus::CallState::error_from_message(DBusMessage& msg)
{
	std::unique_ptr<Error> rc;
	
	DBusMessageIter iter;
	dbus_message_iter_init(&msg, &iter);

	std::string text;
	decode(iter, text);        
						
	rc.reset(new Error);				
	rc->set_members(dbus_message_get_error_name(&msg), text.c_str(), dbus_message_get_reply_serial(&msg));				
	
	return rc;
}


simppl::dbus::CallState::CallState(DBusMessage& msg)
 : ex_()
 , serial_(SIMPPL_INVALID_SERIAL)
{
   if (dbus_message_get_type(&msg) == DBUS_MESSAGE_TYPE_ERROR)
   {	  
	  ex_ = error_from_message(msg);
   }   
   else
      serial_ = dbus_message_get_reply_serial(&msg);
}
