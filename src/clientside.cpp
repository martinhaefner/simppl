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


}   // namespace dbus

}   // namespace simppl

