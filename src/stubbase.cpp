#include "simppl/stub.h"

#include "simppl/dispatcher.h"

#include "simppl/detail/frames.h"
#include "simppl/detail/util.h"

#include <cstring>


namespace {
   
   DBusHandlerResult stub_handle_message(DBusConnecion* conn, DBusMessage* msg, void* data)
   {
      return reinterpret_cast<StubBase*>(data)->handleMessage(msg);
   }
   
   
   DBusObjectPathVTable stub_table = {
      .message_function = stub_handle_essage,
      .unregister_function = nullptr   /* what's this good for? */
   };
}


namespace simppl
{
   
namespace ipc
{


StubBase::StubBase(const char* iface, const char* role, const char* boundname)
 : iface_(iface) 
 , role_(role)
 , disp_(0)
{
   assert(iface);
   assert(role);
   assert(boundname && strlen(boundname) < sizeof(boundname_));
   strcpy(boundname_, boundname);
}


StubBase::~StubBase()
{
   if (disp_)
      disp_->removeClient(*this);
}


Dispatcher& StubBase::disp()
{
   assert(disp_);
   return *disp_;
}


void StubBase::sendSignalRegistration(ClientSignalBase& sigbase)
{
   assert(disp_);
 
   DBusError err;
   dbus_error_init(&err);
      
   dbus_bus_add_match(conn_, "type='signal', sender='role_', interface='interface_', member='sigbase.name_'", &err);
   assert(!dbus_error_is_set(&err));
}


DBusHandlerResult StubBase::handleMessage(DBusMessage* msg)
{
   const char *method = dbus_message_get_member(msg);
	const char *iface = dbus_message_get_interface(msg);

   // TODO lookup map and delegate to appropriate ClientResponse
}


void StubBase::registerObjectPath()
{
    dbus_connection_register_object_path(conn, "role_", &stub_table, this);
}


void StubBase::sendSignalUnregistration(ClientSignalBase& sigbase)
{
   assert(disp_);
   
   DBusError err;
   dbus_error_init(&err);
      
   dbus_bus_remove_match(conn_, "type='signal', sender='role_', interface='interface_', member='sigbase.name_'", &err);
   assert(!dbus_error_is_set(&err));
}


}   // namespace ipc

}   // namespace simppl

