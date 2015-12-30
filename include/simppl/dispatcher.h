#ifndef SIMPPL_DISPATCHER_H
#define SIMPPL_DISPATCHER_H


#include <map>
#include <iostream>
#include <memory>
#include <chrono>
#include <atomic>

#include "simppl/detail/serverholder.h"
#include "simppl/detail/constants.h"

// FIXME can this be removed?
#include "simppl/clientside.h"

#ifdef NDEBUG
#   define safe_cast static_cast
#else
#   define safe_cast dynamic_cast
#endif


// forward decl
struct inotify_event;


namespace simppl
{
   
namespace ipc
{

extern DBusObjectPathVTable stub_v_table;


// forward decls
struct StubBase;
struct ClientResponseBase;
struct ClientSignalBase;

namespace detail
{
   struct Parented;
};


// --------------------------------------------------------------------------------------


// TODO use pimpl for this...
struct Dispatcher
{
   friend struct StubBase;
   
   Dispatcher(const char* bus_name);
   
   template<typename RepT, typename PeriodT>
   inline
   void setRequestTimeout(std::chrono::duration<RepT, PeriodT> duration)
   {
      request_timeout_ = duration;
   }
   
   ~Dispatcher();
   
   template<typename ServerT>
   void addServer(ServerT& serv);
   
   /// Add a client to the dispatcher. This is also necessary if blocking
   /// should be used.
   void addClient(StubBase& clnt);
   
   /// Remove the client.
   void removeClient(StubBase& clnt);
   
   /// no arguments version - will throw exception or error
   void waitForResponse(const detail::ClientResponseHolder& resp);
   
   /// at least one argument version - will throw exception on error
   template<typename T>
   void waitForResponse(const detail::ClientResponseHolder& resp, T& t);

   template<typename... T>
   void waitForResponse(const detail::ClientResponseHolder& resp, std::tuple<T...>& t);
   
   int run();
   
   void stop();
   
   bool isRunning() const;
   
   DBusHandlerResult try_handle_signal(DBusMessage* msg);
   
private:
   
   std::atomic_bool running_;
   
   DBusConnection* conn_;
   
   std::chrono::milliseconds request_timeout_;
   
   std::map<std::string, StubBase*> stubs_;
};


// -----------------------------------------------------------------------------------


template<template<template<typename...> class, 
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,typename> class> class>
struct Skeleton;

// interface checker helper
template<typename ServerT>
struct isServer
{
private:
   
   template<template<template<typename...> class, 
                     template<typename...> class,
                     template<typename...> class,
                     template<typename,typename> class> 
   class IfaceT>
   static int testFunc(const Skeleton<IfaceT>*);

   static char testFunc(...);   
   
public:
   
   enum { value = (sizeof(testFunc((ServerT*)0)) == sizeof(int) ? true : false) };
};


template<typename ServerT>
void Dispatcher::addServer(ServerT& serv)
{
   static_assert(isServer<ServerT>::value, "only_add_servers_here");
   
   serv.disp_ = this;
   
   // isn't this double the information?
   dynamic_cast<detail::BasicInterface*>(&serv)->conn_ = conn_;
   
   dbus_connection_register_object_path(conn_, serv.role_, &stub_v_table, &serv);
   
   /*FIXME
   std::string name = serv.fqn();
   
   assert(std::find_if(servers_by_id_.begin(), servers_by_id_.end(), [&name](decltype(*servers_by_id_.begin())& iter){
      return name == iter.second->fqn();
      }) == servers_by_id_.end());
   
   registerAtBroker(name, endpoints_.front());
   
   detail::ServerHolder<ServerT>* holder = new detail::ServerHolder<ServerT>(serv);
   servers_by_id_[generateId()] = holder;
   */
   serv.disp_ = this;
}


}   // namespace ipc
   
}   // namespace simppl


#endif   // SIMPPL_DISPATCHER_H
