#ifndef SIMPPL_STUBBASE_H
#define SIMPPL_STUBBASE_H


#include <cassert>
#include <functional>

#include "simppl/detail/constants.h"
#include "simppl/detail/parented.h"

#include "simppl/ConnectionState.h"


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
   
   // friendship inheritence
   bool connect(bool block);
   
   inline
   ~StubBase()
   {
      // NOOP
   }

   inline   
   void reparent(detail::Parented* child)
   {
      child->parent_ = this;
   }
   
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
   
   inline
   bool isConnected() const
   {
      return id_ != INVALID_SERVER_ID && fd_ != -1;
   }
   
protected:
   
   uint32_t sendRequest(detail::Parented& requestor, ClientResponseBase* handler, uint32_t requestid, const detail::Serializer& s);
   
   void sendSignalRegistration(ClientSignalBase& sigbase);
   void sendSignalUnregistration(ClientSignalBase& sigbase);
   
   bool isSignalRegistered(ClientSignalBase& sigbase) const;
   
   bool dispatcherIsRunning() const;
   
   inline
   int fd()
   {
      assert(fd_ > 0);
      return fd_;
   }
   
   const char* iface_;
   const char* role_;
   
   char boundname_[24];     ///< where to find the server
   uint32_t id_;            ///< as given from server
   
   Dispatcher* disp_;
   int fd_;                 ///< connected socket
   uint32_t current_sessionid_;
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_STUBBASE_H
