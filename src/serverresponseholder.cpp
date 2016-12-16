#include "simppl/detail/serverresponseholder.h"

#include "simppl/detail/serialization.h"


namespace simppl
{
   
namespace dbus
{
   
namespace detail
{
   
ServerResponseHolder::ServerResponseHolder(std::function<void(Serializer&)> f)
 : f_(f)
{
   // NOOP
}


ServerResponseHolder::ServerResponseHolder(ServerResponseHolder&& rhs)
 : f_(std::move(rhs.f_))
{
   // NOOP
}


ServerResponseHolder::~ServerResponseHolder()
{
   // NOOP
}


ServerResponseHolder& ServerResponseHolder::operator=(ServerResponseHolder&& rhs)
{
   if (this != &rhs)
      f_ = rhs.f_;
   
   return *this;
}

}   // namespace detail

}   // namespace dbus

}   // namespace simppl
