#include "simppl/detail/serverresponseholder.h"

#include "simppl/detail/serialization.h"


namespace simppl
{
   
namespace dbus
{
   
namespace detail
{
   
ServerResponseHolder::ServerResponseHolder(ServerResponseBase& responder, std::function<void(Serializer&)> f)
 : responder_(&responder)
 , f_(f)
{
   // NOOP
}


ServerResponseHolder::ServerResponseHolder(ServerResponseHolder&& rhs)
 : responder_(rhs.responder_)
 , f_(rhs.f_)
{
   rhs.responder_ = nullptr;
   // FIXME implement real move semantics
}


ServerResponseHolder::~ServerResponseHolder()
{
   // NOOP
}


ServerResponseHolder& ServerResponseHolder::operator=(ServerResponseHolder&& rhs)
{
   if (this != &rhs)
   {
      responder_ = rhs.responder_;
      f_ = rhs.f_;

      rhs.responder_ = nullptr;
   }
   
   return *this;
}

}   // namespace detail

}   // namespace dbus

}   // namespace simppl
