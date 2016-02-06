#ifndef SIMPPL_SERVERREQUESTDESCRIPTOR_H
#define SIMPPL_SERVERREQUESTDESCRIPTOR_H


#include <stdint.h>


// forward decl
struct DBusMessage;


namespace simppl
{
   
namespace dbus
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

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_SERVERREQUESTDESCRIPTOR_H
