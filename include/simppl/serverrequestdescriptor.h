#ifndef SIMPPL_SERVERREQUESTDESCRIPTOR_H
#define SIMPPL_SERVERREQUESTDESCRIPTOR_H


#include <stdint.h>


namespace simppl
{
   
namespace ipc
{
   
// forward decl
struct ServerRequestBase;


struct ServerRequestDescriptor
{
   ServerRequestDescriptor();
   
   ServerRequestDescriptor(const ServerRequestDescriptor& rhs) = delete;
   ServerRequestDescriptor& operator=(const ServerRequestDescriptor& rhs) = delete;
   
   ServerRequestDescriptor(ServerRequestDescriptor&& rhs);
   ServerRequestDescriptor& operator=(ServerRequestDescriptor&& rhs);
   
   ServerRequestDescriptor& set(ServerRequestBase* requestor, int fd, uint32_t sequence_nr, uint32_t sessionid);
   
   void clear();
   
   operator const void*() const;
   
   ServerRequestBase* requestor_;
   int fd_;
   uint32_t sequence_nr_;
   uint32_t sessionid_;
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_SERVERREQUESTDESCRIPTOR_H
