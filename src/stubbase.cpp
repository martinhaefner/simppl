#include "simppl/stub.h"

#include "simppl/dispatcher.h"

#include "simppl/detail/frames.h"
#include "simppl/detail/util.h"

#include <cstring>


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
   
   // FIXME do we really need dependency to dispatcher?
   dbus_bus_add_match(disp_->conn_, "type='signal', sender='role_', interface='interface_', member='sigbase.name_'", &err);
   assert(!dbus_error_is_set(&err));
   
   // FIXME handle double attach
   signals_[sigbase.name()] = &sigbase;
}


DBusHandlerResult StubBase::try_handle_signal(DBusMessage* msg)
{
   const char *method = dbus_message_get_member(msg);

   auto iter = signals_.find(dbus_message_get_member(msg));
   if (iter != signals_.end())
   {
       iter->second->eval(msg);
       return DBUS_HANDLER_RESULT_HANDLED;
   }
   
   return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


void StubBase::sendSignalUnregistration(ClientSignalBase& sigbase)
{
   assert(disp_);
   
   DBusError err;
   dbus_error_init(&err);
      
   dbus_bus_remove_match(disp_->conn_, "type='signal', sender='role_', interface='interface_', member='sigbase.name_'", &err);
   assert(!dbus_error_is_set(&err));
   
   // FIXME remove signalbase handler again
}


}   // namespace ipc

}   // namespace simppl

