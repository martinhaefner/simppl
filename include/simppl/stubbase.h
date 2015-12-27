#ifndef SIMPPL_STUBBASE_H
#define SIMPPL_STUBBASE_H


#include <cassert>
#include <functional>

#include "simppl/detail/constants.h"
#include "simppl/detail/parented.h"

#include "simppl/connectionstate.h"


namespace simppl
{
   
namespace ipc
{

// forward decls
struct Dispatcher;
struct ClientResponseBase;
struct ClientSignalBase;

namespace detail
{
   struct Serializer;
};


struct StubBase
{
   template<typename... T> friend struct ClientSignal;
   template<typename... T> friend struct ClientRequest;
   friend struct Dispatcher;
   
protected:
   
   ~StubBase();
   
public:
   
   std::function<void(ConnectionState)> connected;
   
   StubBase(const char* iface, const char* role, const char* boundname);
   
   inline
   const char* iface() const
   {
      return iface_;
   }
   
   inline
   const char* role() const
   {
      return role_;
   }
   
   Dispatcher& disp();
   
protected:
   
   void sendSignalRegistration(ClientSignalBase& sigbase);
   void sendSignalUnregistration(ClientSignalBase& sigbase);
   
   void register_object_path();
   DBusHandlerResult handleMessage(DBusMessage* msg);
   
   const char* iface_;
   const char* role_;
   
   char boundname_[24];     ///< where to find the server
   
   Dispatcher* disp_;
   int fd_;                 ///< connected socket
   uint32_t current_sessionid_;
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_STUBBASE_H
