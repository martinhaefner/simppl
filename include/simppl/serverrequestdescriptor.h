#ifndef SIMPPL_SERVERREQUESTDESCRIPTOR_H
#define SIMPPL_SERVERREQUESTDESCRIPTOR_H


#include <stdint.h>


// forward decl
struct DBusMessage;


namespace simppl
{
   
namespace ipc
{
   
// forward decl
struct ServerRequestBase;


struct ServerRequestDescriptor
{
   ServerRequestDescriptor();
   ~ServerRequestDescriptor();
   
   ServerRequestDescriptor(const ServerRequestDescriptor& rhs) = delete;
   ServerRequestDescriptor& operator=(const ServerRequestDescriptor& rhs) = delete;
   
   ServerRequestDescriptor(ServerRequestDescriptor&& rhs);
   ServerRequestDescriptor& operator=(ServerRequestDescriptor&& rhs);
   
   ServerRequestDescriptor& set(ServerRequestBase* requestor, DBusMessage* msg);
   
   void clear();
   
   operator const void*() const;
   
   ServerRequestBase* requestor_;
   DBusMessage* msg_;
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_SERVERREQUESTDESCRIPTOR_H
