#include "simppl/serialization.h"


void simppl_dbus_message_iter_recurse(DBusMessageIter* iter, DBusMessageIter* nested, int expected_type)
{
   if (dbus_message_iter_get_arg_type(iter) != expected_type)
      throw simppl::dbus::DecoderError();
      
   dbus_message_iter_recurse(iter, nested);
}


void simppl_dbus_message_iter_get_basic(DBusMessageIter* iter, void* p, int expected_type)
{
   if (dbus_message_iter_get_arg_type(iter) != expected_type)
      throw simppl::dbus::DecoderError();
   
   dbus_message_iter_get_basic(iter, p);
   dbus_message_iter_next(iter);
}


namespace simppl
{

namespace dbus
{

namespace
{
   thread_local DBusMessage* current_decoding_message = nullptr;
}


DecodingScope::DecodingScope(DBusMessage* msg)
 : prev_(current_decoding_message)
{
   current_decoding_message = msg;
}


DecodingScope::~DecodingScope()
{
   current_decoding_message = prev_;
}


/*static*/
DBusMessage* DecodingScope::current()
{
   return current_decoding_message;
}


}   // namespace dbus

}   // namespace simppl
