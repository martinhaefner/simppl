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

namespace dbus
{

extern DBusObjectPathVTable stub_v_table;


// forward decls
struct StubBase;
struct SkeletonBase;
struct ClientSignalBase;


// TODO use pimpl for this...
struct Dispatcher
{
   friend struct StubBase;
   friend struct SkeletonBase;

   Dispatcher(const Dispatcher&) = delete;
   Dispatcher& operator=(const Dispatcher&) = delete;

   /**
    * @param busname the busname to use, e.g. "bus:session" or "bus:system. nullptr means session.
    *                Attaching to TCP is done via "tcp:host=<ip>,port=<port>" Any other dbus compatible
    *                format may be used.
    */
   Dispatcher(const char* busname = nullptr);

   ~Dispatcher();

   /// Add a client to the dispatcher. This is also necessary if blocking
   /// should be used.
   void addClient(StubBase& clnt);

   /// Remove the client.
   void removeClient(StubBase& clnt);

   /// add a server
   void addServer(SkeletonBase& server);

   /// no arguments version - will throw exception or error
   void waitForResponse(const detail::ClientResponseHolder& resp);

   /// at least one argument version - will throw exception on error
   template<typename T>
   void waitForResponse(const detail::ClientResponseHolder& resp, T& t);

   /// more than one argument version
   template<typename... T>
   void waitForResponse(const detail::ClientResponseHolder& resp, std::tuple<T...>& t);

   template<typename T, int Flags>
   void waitForResponse(ClientAttribute<T, Flags>& resp, T& t);

   template<typename RepT, typename PeriodT>
   inline
   void setRequestTimeout(std::chrono::duration<RepT, PeriodT> duration)
   {
      request_timeout_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
   }

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

   inline
   int request_timeout() const
   {
      return request_timeout_;
   }

   /// propagate exception
   void propagate(CallState state);

   inline
   DBusConnection& connection()
   {
      return *conn_;
   }

   void dispatch();

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


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DISPATCHER_H
