#include "simppl/detail/serverresponseholder.h"

#include "simppl/detail/serialization.h"


namespace simppl
{
   
namespace ipc
{
   
namespace detail
{
   
ServerResponseHolder::ServerResponseHolder(DBusMessage* response, ServerResponseBase& responder)
 : response_(response)
 , responder_(&responder)
{
   // NOOP
}


ServerResponseHolder::ServerResponseHolder(ServerResponseHolder&& rhs)
 : response_(rhs.response_)
 , responder_(rhs.responder_)
{
   rhs.response_ = nullptr;
   rhs.responder_ = nullptr;
}


ServerResponseHolder::~ServerResponseHolder()
{
   // NOOP
}


ServerResponseHolder& ServerResponseHolder::operator=(ServerResponseHolder&& rhs)
{
   if (this != &rhs)
   {
      response_ = rhs.response_;
      responder_ = rhs.responder_;

      rhs.response_ = nullptr;
      rhs.responder_ = nullptr;
   }
   
   return *this;
}

}   // namespace detail

}   // namespace ipc

}   // namespace simppl
