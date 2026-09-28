#include "simppl/interface.h"

#define SIMPPL_SERVERSIDE_CPP
#include "simppl/serverside.h"
#undef SIMPPL_SERVERSIDE_CPP
 
 
namespace simppl
{
 
namespace dbus
{
 
 
ServerMethodBase::ServerMethodBase(const char* name, SkeletonBase* iface, int iface_id)
 : name_(name)
{
   auto& methods = iface->method_heads_[iface_id];
   this->next_ = methods;
   methods = this;
}


ServerMethodBase::~ServerMethodBase()
{
   // NOOP
}



ServerPropertyBase::ServerPropertyBase(const char* name, SkeletonBase* iface, int iface_id)
 : name_(name)
 , iface_id_(iface_id)
 , parent_(iface)
{
   auto& properties = iface->property_heads_[iface_id];
   this->next_ = properties;
   properties = this;
}


ServerSignalBase::ServerSignalBase(const char* name, SkeletonBase* iface, int iface_id)
 : name_(name)
 , iface_id_(iface_id)
 , parent_(iface)
{
#if SIMPPL_HAVE_INTROSPECTION
   auto& signals = iface->signal_heads_[iface_id];
   this->next_ = signals;
   signals = this;
#endif
}
 
 
}   // namespace dbus

}   // namespace simppl
