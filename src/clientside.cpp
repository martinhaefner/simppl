#include "simppl/clientside.h"


namespace simppl
{

namespace dbus
{


ClientSignalBase::ClientSignalBase(const char* name, StubBase* stub, int)
 : stub_(stub)
 , name_(name)
{
   // NOOP
}


ClientPropertyBase::ClientPropertyBase(const char* name, StubBase* stub, int)
 : name_(name)
 , stub_(stub)
{
   stub_->add_property(this);
}


/// only call this after the server is connected.
void ClientPropertyBase::detach()
{
   stub_->detach_property(this);
}


}   // namespace dbus

}   // namespace simppl

