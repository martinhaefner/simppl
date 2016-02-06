#ifndef SIMPPL_DETAIL_SERVERRESPONSEHOLDER_H
#define SIMPPL_DETAIL_SERVERRESPONSEHOLDER_H


#include <functional>
#include <cstdint>

#include <sys/types.h>


// forward decl
struct DBusMessage;


namespace simppl
{
   
namespace dbus
{

// forward decl
struct ServerResponseBase;


namespace detail
{

struct Serializer;


struct ServerResponseHolder 
{
   ServerResponseHolder(ServerResponseBase& responder, std::function<void(Serializer&)> f);
   
   ~ServerResponseHolder();
   
   ServerResponseHolder(ServerResponseHolder&& rhs);
   ServerResponseHolder& operator=(ServerResponseHolder&& rhs);
   
   ServerResponseHolder(const ServerResponseHolder& rhs) = delete;
   ServerResponseHolder& operator=(const ServerResponseHolder& rhs) = delete;
   
   /*mutable*/ ServerResponseBase* responder_;
   std::function<void(Serializer&)> f_;
};

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERVERRESPONSEHOLDER_H
