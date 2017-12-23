#include "simppl/interface.h"

#define SIMPPL_SERVERSIDE_CPP
#include "simppl/serverside.h"
#undef SIMPPL_SERVERSIDE_CPP
 
 
namespace simppl
{
 
namespace dbus
{
 
 
ServerMethodBase::ServerMethodBase(const char* name, SkeletonBase* iface)
 : name_(name)
{
   auto& methods = iface->methods_;
   this->next_ = methods;
   methods = this;
}


ServerMethodBase::~ServerMethodBase()
{
   // NOOP
}


#if SIMPPL_SIGNATURE_CHECK
const char* ServerMethodBase::get_signature() const
{
   if (signature_.empty())
   {
      std::ostringstream oss;
      oss << "sig:";
      get_signature(oss);
      
      signature_ = oss.str();
   }
   
   return signature_.c_str()+4;
}
#endif

 
ServerPropertyBase::ServerPropertyBase(const char* name, SkeletonBase* iface)
 : name_(name)
 , parent_(iface)
{
   auto& properties = iface->properties_;
   this->next_ = properties;
   properties = this;
}


ServerSignalBase::ServerSignalBase(const char* name, SkeletonBase* iface)
 : name_(name)
 , parent_(iface)
{
#if SIMPPL_HAVE_INTROSPECTION
   auto& signals = iface->signals_;
   this->next_ = signals;
   signals = this;
#endif
}
 
 
}   // namespace dbus

}   // namespace simppl
