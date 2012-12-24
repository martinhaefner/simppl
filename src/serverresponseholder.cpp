#include "simppl/detail/serverresponseholder.h"

#include "simppl/detail/serialization.h"


namespace simppl
{
   
namespace ipc
{
   
namespace detail
{
   
ServerResponseHolder::ServerResponseHolder(Serializer& s, ServerResponseBase& responder)
 : size_(s.size())
 , payload_(s.release())
 , responder_(&responder)
{
   // NOOP
}


ServerResponseHolder::ServerResponseHolder(ServerResponseHolder&& rhs)
 : payload_(rhs.payload_)
 , size_(rhs.size_)
 , responder_(rhs.responder_)
{
   rhs.payload_ = nullptr;
   rhs.size_ = 0;
}


ServerResponseHolder::~ServerResponseHolder()
{
   Serializer::free(payload_);
}


ServerResponseHolder& ServerResponseHolder::operator=(ServerResponseHolder&& rhs)
{
   if (this != &rhs)
   {
      payload_ = rhs.payload_;
      size_ = rhs.size_;
      responder_ = rhs.responder_;

      rhs.payload_ = nullptr;
      rhs.size_ = 0;
      rhs.responder_ = nullptr;
   }
   
   return *this;
}

}   // namespace detail

}   // namespace ipc

}   // namespace simppl
