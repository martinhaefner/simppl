#include "simppl/detail/serversignalbase.h"

#include "simppl/interface.h"

#define SIMPPL_SERVERSIGNALBASE_CPP
#include "simppl/serverside.h"
#undef SIMPPL_SERVERSIGNALBASE_CPP


namespace simppl
{

namespace ipc
{


// FIXME move to somewhere different
ServerRequestBase::ServerRequestBase(const char* name, detail::BasicInterface* iface)
 : hasResponse_(false)
 , name_(name)
{
   dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->methods_[name_] = this;
}


ServerAttributeBase::ServerAttributeBase(const char* name, detail::BasicInterface* iface)
 : name_(name)
{
   dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->attributes_[name_] = this;
}


namespace detail
{

ServerSignalBase::~ServerSignalBase()
{
   // NOOP
}

}   // namespace detail

}   // namespace ipc

}   // namespace simppl
