#include "simppl/stub.h"

#include "simppl/dispatcher.h"

#include "simppl/detail/frames.h"
#include "simppl/detail/util.h"

#include <cstring>
#include <sstream>


namespace simppl
{
   
namespace ipc
{


StubBase::StubBase(const char* iface, const char* role, const char* boundname)
 : role_(role)
 , disp_(0)
{
   assert(iface);
   assert(role);
   assert(boundname && strlen(boundname) < sizeof(boundname_));
   
   // strip template arguments
   memset(iface_, 0, sizeof(iface_));
   strncpy(iface_, iface, strstr(iface, "<") - iface);
   
   // remove '::' separation in favour of '.' separation
   char *readp = iface_, *writep = iface_;
   while(*readp)
   {
      if (*readp == ':')
      {
         *writep++ = '.';
         readp += 2;
      }
      else
         *writep++ = *readp++; 
   }
   
   // terminate
   *writep = '\0';
   
   strcpy(boundname_, boundname);
}


StubBase::~StubBase()
{
   if (disp_)
      disp_->removeClient(*this);
}


DBusConnection* StubBase::conn()
{
    return disp().conn_;
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
   std::ostringstream match_string;
   match_string << "type='signal'";
   match_string << ",sender='" << boundname_ << "'";
   match_string << ",interface='" << iface() << "'";
   match_string << ",member='" << sigbase.name() << "'";
   
   dbus_bus_add_match(disp_->conn_, match_string.str().c_str(), &err);
   //assert(!dbus_error_is_set(&err));
   if (dbus_error_is_set(&err))
       std::cerr << err.name << " " << err.message << std::endl;
   
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
   
   std::ostringstream match_string;
   match_string << "type='signal'";
   match_string << ", sender='" << role() << "'";
   match_string << ", interface='" << iface() << "'";
   match_string << ", member='" << sigbase.name() << "'";
   
   dbus_bus_remove_match(disp_->conn_, match_string.str().c_str(), &err);
   assert(!dbus_error_is_set(&err));
   
   // FIXME remove signalbase handler again
}


}   // namespace ipc

}   // namespace simppl

