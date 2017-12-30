#include "simppl/clientside.h"


namespace simppl
{
   
namespace dbus
{
   

ClientSignalBase::ClientSignalBase(const char* name, StubBase* stub)
 : stub_(stub)
 , name_(name)
 , next_(nullptr)
{
   // NOOP
}


ClientPropertyBase::ClientPropertyBase(const char* name, StubBase* stub)
 : name_(name)
 , stub_(stub)
 , next_(nullptr)
{
   // NOOP
}


/// only call this after the server is connected.
void ClientPropertyBase::detach()
{
   stub_->detach_property(*this);
}


}   // namespace dbus

}   // namespace simppl

