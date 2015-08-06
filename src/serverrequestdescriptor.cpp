#include "simppl/serverrequestdescriptor.h"

#include "simppl/detail/constants.h"


namespace simppl
{
   
namespace ipc
{


ServerRequestDescriptor::ServerRequestDescriptor()
 : requestor_(0)
 , fd_(-1)
 , sequence_nr_(INVALID_SEQUENCE_NR)
 , sessionid_(INVALID_SESSION_ID)
{
   // NOOP
}


ServerRequestDescriptor::ServerRequestDescriptor(const ServerRequestDescriptor& rhs)
 : requestor_(rhs.requestor_)
 , fd_(rhs.fd_)
 , sequence_nr_(rhs.sequence_nr_)
 , sessionid_(rhs.sessionid_)
{
   const_cast<ServerRequestDescriptor&>(rhs).clear();
}


ServerRequestDescriptor& ServerRequestDescriptor::operator=(const ServerRequestDescriptor& rhs)
{
   if (this != &rhs)
   {
      requestor_ = rhs.requestor_;
      fd_ = rhs.fd_;
      sequence_nr_ = rhs.sequence_nr_;
      sessionid_ = rhs.sessionid_;
      
      const_cast<ServerRequestDescriptor&>(rhs).clear();
   }
   
   return *this;
}

ServerRequestDescriptor& ServerRequestDescriptor::set(ServerRequestBase* requestor, int fd, uint32_t sequence_nr, uint32_t sessionid)
{
   requestor_ = requestor;
   fd_ = fd;
   sequence_nr_ = sequence_nr;
   sessionid_ = sessionid;
   return *this;
}


void ServerRequestDescriptor::clear()
{
   set(0, -1, INVALID_SEQUENCE_NR, INVALID_SESSION_ID);
}


ServerRequestDescriptor::operator const void*() const
{
   return requestor_;
}


}   // namespace ipc

}   // namespace simppl

