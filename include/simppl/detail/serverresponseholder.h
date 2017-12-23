#ifndef SIMPPL_DETAIL_SERVERRESPONSEHOLDER_H
#define SIMPPL_DETAIL_SERVERRESPONSEHOLDER_H


#include <functional>
#include <cstdint>

#include <sys/types.h>


// forward decl
struct DBusMessage;
struct DBusMessageIter;


namespace simppl
{
   
namespace dbus
{

namespace detail
{


struct ServerResponseHolder 
{
   ServerResponseHolder(std::function<void(DBusMessageIter&)> f);
   
   ~ServerResponseHolder();
   
   ServerResponseHolder(ServerResponseHolder&& rhs);
   ServerResponseHolder& operator=(ServerResponseHolder&& rhs);
   
   ServerResponseHolder(const ServerResponseHolder& rhs) = delete;
   ServerResponseHolder& operator=(const ServerResponseHolder& rhs) = delete;
   
   std::function<void(DBusMessageIter&)> f_;
};

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERVERRESPONSEHOLDER_H
