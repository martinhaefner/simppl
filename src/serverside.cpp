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
   dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->methods_[name_] = this;
}
 
 
ServerAttributeBase::ServerAttributeBase(const char* name, detail::BasicInterface* iface)
 : name_(name)
{
   dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->attributes_[name_] = this;
}


ServerSignalBase::ServerSignalBase(const char* name, detail::BasicInterface* iface)
 : name_(name)
 , parent_(iface)
{
#if SIMPPL_HAVE_INTROSPECTION
   dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->signals_[name_] = this;
#endif
}
 
 
}   // namespace dbus

}   // namespace simppl
