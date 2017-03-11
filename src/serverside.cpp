#include "simppl/detail/serialization.h"
#include "simppl/interface.h"

#define SIMPPL_SERVERSIDE_CPP
#include "simppl/serverside.h"
#undef SIMPPL_SERVERSIDE_CPP
 
 
namespace simppl
{
 
namespace dbus
{
 
 
ServerRequestBase::ServerRequestBase(const char* name, detail::BasicInterface* iface)
 : name_(name)
{
   auto& methods = dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->methods_;
   this->next_ = methods;
   methods = this;
}
 
 
ServerPropertyBase::ServerPropertyBase(const char* name, detail::BasicInterface* iface)
 : name_(name)
 , parent_(iface)
{
   auto& properties = dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->properties_;
   this->next_ = properties;
   properties = this;
}


ServerSignalBase::ServerSignalBase(const char* name, detail::BasicInterface* iface)
 : name_(name)
 , parent_(iface)
{
#if SIMPPL_HAVE_INTROSPECTION
   auto& signals = dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->signals_;
   this->next_ = signals;
   signals = this;
#endif
}
 
 
}   // namespace dbus

}   // namespace simppl
