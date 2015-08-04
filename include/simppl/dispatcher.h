#ifndef SIMPPL_DISPATCHER_H
#define SIMPPL_DISPATCHER_H


#include <map>
#include <iostream>
#include <memory>
#include <chrono>
#include <atomic>

#include <sys/poll.h>

#include "simppl/detail/serverholder.h"
#include "simppl/detail/constants.h"

#include "simppl/sessiondata.h"
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

// forward decls
struct BrokerClient;
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
   
   /// boundname may be 0 for handling clients without any servers
   Dispatcher(const char* boundname = nullptr);
   
   /// allow dispatcher to talk to broker for registering new services and to
   /// wait for clients to attach services
   void enableBrokerage();
   
   template<typename RepT, typename PeriodT>
   inline
   void setRequestTimeout(std::chrono::duration<RepT, PeriodT> duration)
   {
      request_timeout_ = duration;
   }
   
   virtual ~Dispatcher();
   
   virtual void socketConnected(int /*fd*/);
   
   virtual void socketDisconnected(int /*fd*/);
   
   /// attach multiple transport endpoints (e.g. tcp socket or datagram transport endpoint)
   /// e.g.   attach("unix:/server1")
   ///        attach("tcp:127.0.0.1:8888")
   bool attach(const char* endpoint);
   
   template<typename ServerT>
   void addServer(ServerT& serv);
   
   inline
   uint32_t generateId()
   {
      return ++nextid_;
   }
   
   inline
   void addRequest(detail::Parented& req, ClientResponseBase& resp, uint32_t sequence_nr, int outfd)
   {
      pendings_[sequence_nr] = std::make_tuple(&req, &resp, dueTime(), outfd);
   }
   
   bool addSignalRegistration(ClientSignalBase& s, uint32_t sequence_nr);

   /// @return 0 if signal is not registered, the id otherwise
   uint32_t removeSignalRegistration(ClientSignalBase& s);
   
   /// this function is !not! thread-safe
   inline
   uint32_t generateSequenceNr() 
   {
      ++sequence_;
      return sequence_ == INVALID_SEQUENCE_NR ? ++sequence_ : sequence_;
   }
   
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
   
   static std::string fullQualifiedName(const char* ifname, const char* rolename);
   
   
private:

   int loop(unsigned int timeoutMs = 100);  ///< FIXME timeout must be somehow dynamic -> til next time_point 
   int once(unsigned int timeoutMs);

   void handle_blocking_connect(ConnectionState s, StubBase* stub, uint32_t seqNr);

   void checkPendings();
   void removePendingsForFd(int fd);
   
   /// calculate duetime for request
   std::chrono::steady_clock::time_point dueTime() const;
   
   void serviceReady(StubBase* stub, const std::string& fullName, const std::string& location);
   
   int accept_socket(int acceptor, short pollmask);
   int handle_data(int fd, short pollmask);
   int handle_inotify(int fd, short pollmask);
   

public:
   
   int run();
   
   bool isSignalRegistered(ClientSignalBase& sigbase) const;
   
   void clearSlot(int idx);
   
   void stop();
   
   bool isRunning() const;
   
   void* getSessionData(uint32_t sessionid);
   
   void registerSession(uint32_t fd, uint32_t sessionid, void* data, void(*destructor)(void*));
   
   /// register arbitrary file descriptors, e.g. timers
   void register_fd(int fd, short pollmask, std::function<int(int, short)> cb);
   
   /// user-specific fd
   void unregister_fd(int fd);
   
   
private:

   void registerAtBroker(const std::string& service, const std::string& endpoint);

   void connect(StubBase& stub, const char* location = 0);

   uint32_t send_resolve_interface(StubBase& stub);
   void handle_inotify_event(struct inotify_event* evt);
   void add_inotify_location(StubBase& stub, const char* socketpath);
   
   /// inotify based lookups for handling startup race conditions. This
   /// only works for unix: based services. 
   int inotify_fd_;
   std::multimap<std::string, std::tuple<StubBase*, std::chrono::steady_clock::time_point/*=duetime*/>> pending_lookups_;
   
   std::map<uint32_t/*=serverid*/, detail::ServerHolderBase*> servers_by_id_;
   
   std::map<uint32_t/*=sequencenr*/, std::tuple<StubBase*, std::chrono::steady_clock::time_point/*=duetime*/>> pending_interface_resolves_;
   
   uint32_t nextid_;
   std::atomic_bool running_;
   
   pollfd fds_[32];
   
   /// iohandler callback functions
   std::map<int, std::function<int(int, short)>> fctn_;
   
   /// registered clients
   std::multimap<std::string/*=server::role the client is connected to*/, StubBase*> clients_;
   
   uint32_t sequence_;
   
   std::map<
      uint32_t/*=sequencenr*/, 
      std::tuple<detail::Parented*, 
                ClientResponseBase*, 
                std::chrono::steady_clock::time_point/*=duetime*/, 
                int/*outfd*/>
      > pendings_;
   
   /// temporary maps, should always shrink again, maybe could drop entries on certain timeouts
   std::map<uint32_t/*=sequencenr*/, ClientSignalBase*> outstanding_sig_registrs_;
   
   /// signal handling client- and server-side   
   std::map<uint32_t/*=clientside_id*/, ClientSignalBase*> sighandlers_;   
   std::map<uint32_t/*=serverside_id*/, detail::ServerSignalBase*> server_sighandlers_;
   
   /// FIXME need refcounting here!
   /// all client side socket connections (good for multiplexing) FIXME need refcounting and reconnect strategy
   std::map<std::string/*boundname*/, int/*=fd*/> socks_;
   
   BrokerClient* broker_;
   std::vector<std::string> endpoints_;
   
   std::map<uint32_t/*=sessionid*/, SessionData> sessions_;
   
   std::chrono::milliseconds request_timeout_;
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
   assert(!endpoints_.empty());
   
   std::string name = serv.fqn();
   
   assert(std::find_if(servers_by_id_.begin(), servers_by_id_.end(), [&name](decltype(*servers_by_id_.begin())& iter){
      return name == iter.second->fqn();
      }) == servers_by_id_.end());
   
   registerAtBroker(name, endpoints_.front());
   
   detail::ServerHolder<ServerT>* holder = new detail::ServerHolder<ServerT>(serv);
   servers_by_id_[generateId()] = holder;
   
   serv.disp_ = this;
}


}   // namespace ipc
   
}   // namespace simppl


#endif   // SIMPPL_DISPATCHER_H
