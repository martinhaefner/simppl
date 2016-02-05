#ifndef SIMPPL_DISPATCHER_H
#define SIMPPL_DISPATCHER_H


#include <map>
#include <set>
#include <iostream>
#include <memory>
#include <chrono>
#include <atomic>
#include <queue>

#include "simppl/detail/constants.h"

// FIXME can this be remroved?
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
   friend struct SkeletonBase;

   Dispatcher(const Dispatcher&) = delete;
   Dispatcher& operator=(const Dispatcher&) = delete;

   /**
    * @param busname the busname to use, e.g. "dbus:session" or "dbus:system. 0 is session.
    */
   Dispatcher(const char* busname = 0);

   template<typename RepT, typename PeriodT>
   inline
   void setRequestTimeout(std::chrono::duration<RepT, PeriodT> duration)
   {
      request_timeout_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
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

   /**
    * Do some IO and dispatch the retrieved messages.
    */
   template<typename RepT, typename PeriodT>
   inline
   int step(std::chrono::duration<RepT, PeriodT> duration)
   {
       return step_ms(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
   }

   /// same as run()
   void loop();

   void stop();

   bool isRunning() const;

   DBusHandlerResult try_handle_signal(DBusMessage* msg);

   void registerSignal(StubBase& stub, ClientSignalBase& sigbase);
   void unregisterSignal(StubBase& stub, ClientSignalBase& sigbase);

   int request_timeout() const
   {
      return request_timeout_;
   }
   
   /// propagate exception
   void propagate(const CallState& state);

private:

   int step_ms(int millis);

   void notify_clients(const std::string& boundname, ConnectionState state);

   std::atomic_bool running_;

   DBusConnection* conn_;

   int request_timeout_;    ///< default request timeout in milliseconds

   std::multimap<std::string, StubBase*> stubs_;

   std::map<std::string, int> signal_matches_;

   /// service registration's list
   std::set<std::string> busnames_;

   std::queue<CallState> exceptions_;

   struct Private;
   Private* d;
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

   // FIXME move most code to .cpp
   DBusError err;
   dbus_error_init(&err);

   std::ostringstream busname;
   busname << serv.iface_ << '.' << serv.role_;

   dbus_bus_request_name(conn_, busname.str().c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
   assert(!dbus_error_is_set(&err));

   // isn't this double the information?
   dynamic_cast<detail::BasicInterface*>(&serv)->conn_ = conn_;

   std::string objectpath = "/" + busname.str();
   std::for_each(objectpath.begin(), objectpath.end(), [](char& c){
      if (c == '.')
         c = '/';
   });

   // register same path as busname, just with / instead of .
   dbus_connection_register_object_path(conn_, objectpath.c_str(), &stub_v_table, &serv);

//std::cout << this << ": added server '" << busname.str() << "'" << std::endl;
   serv.disp_ = this;
}


}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DISPATCHER_H
