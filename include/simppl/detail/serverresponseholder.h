#ifndef SIMPPL_DETAIL_SERVERRESPONSEHOLDER_H
#define SIMPPL_DETAIL_SERVERRESPONSEHOLDER_H


#include <cstdint>
#include <sys/types.h>


// forward decl
struct DBusMessage;


namespace simppl
{
   
namespace ipc
{

// forward decl
struct ServerResponseBase;


namespace detail
{

struct Serializer;


struct ServerResponseHolder 
{
   ServerResponseHolder(DBusMessage* response, ServerResponseBase& responder);
   
   ~ServerResponseHolder();
   
   ServerResponseHolder(ServerResponseHolder&& rhs);
   ServerResponseHolder& operator=(ServerResponseHolder&& rhs);
   
   ServerResponseHolder(const ServerResponseHolder& rhs) = delete;
   ServerResponseHolder& operator=(const ServerResponseHolder& rhs) = delete;
   
   /*mutable*/ DBusMessage* response_;
   /*mutable*/ ServerResponseBase* responder_;
};

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERVERRESPONSEHOLDER_H
