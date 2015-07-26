#ifndef SIMPPL_DISPATCHER_H
#define SIMPPL_DISPATCHER_H


#include <map>
#include <iostream>
#include <memory>
#include <chrono>
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


template<int N, typename TupleT>
inline
void assign(const TupleT& tuple)
{
   // NOOP - end condition
}

template<int N, typename TupleT, typename T1, typename... T>
inline
void assign(const TupleT& tuple, T1& t1, T&... t)
{
   t1 = std::move(std::get<N>(tuple));
   assign<N+1>(tuple, t...);
}


// --------------------------------------------------------------------------------------


extern std::unique_ptr<char> NullUniquePtr;


struct Dispatcher
{
   friend struct StubBase;
   
   // resolve server name to id
   typedef std::map<std::string/*=server::role*/, detail::ServerHolderBase*> servermap_type;  
   
   // signal registration and request resolution
   typedef std::map<uint32_t/*=serverid*/, detail::ServerHolderBase*> servermapid_type;
   
   // all registered clients
   typedef std::multimap<std::string/*=server::role the client is connected to*/, StubBase*> clientmap_type;
   
   // all client side socket connections (good for multiplexing) FIXME need refcounting and reconnect strategy
   typedef std::map<std::string/*boundname*/, int/*=fd*/> socketsmap_type;
   
   // temporary maps, should always shrink again, maybe could drop entries on certain timeouts
   typedef std::map<uint32_t/*=sequencenr*/, ClientSignalBase*> outstanding_signalregistrations_type;
   
   // signal handling client- and server-side
   typedef std::map<uint32_t/*=clientside_id*/, ClientSignalBase*> sighandlers_type;
   typedef std::map<uint32_t/*=serverside_id*/, detail::ServerSignalBase*> serversignalers_type;
   
   typedef std::map<uint32_t/*=sessionid*/, SessionData> sessionmap_type;
   
   
   /// boundname may be 0 for handling clients without any servers
   Dispatcher(const char* boundname = nullptr);
   
   /// allow clients to send requests from threads other the dispatcher is running in
   void enableThreadedClient()
   {
      // FIXME
   }
   
   /// allow dispatcher to talk to broker for registering new services and to
   /// wait for clients to attach services
   void enableBrokerage();
   
   template<typename RepT, typename PeriodT>
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
   void addRequest(detail::Parented& req, ClientResponseBase& resp, uint32_t sequence_nr)
   {
      pendings_[sequence_nr] = std::make_tuple(&req, &resp, dueTime());
   }
   
   bool addSignalRegistration(ClientSignalBase& s, uint32_t sequence_nr);

   /// @return 0 if signal is not registered, the id otherwise
   uint32_t removeSignalRegistration(ClientSignalBase& s);
   
   uint32_t generateSequenceNr() 
   {
      ++sequence_;
      return sequence_ == INVALID_SEQUENCE_NR ? ++sequence_ : sequence_;
   }
   
   void addClient(StubBase& clnt);
   
   /// no arguments version - will throw exception or error
   bool waitForResponse(const detail::ClientResponseHolder& resp);
   
   /// at least one argument version - will throw exception on error
   template<typename T1, typename... T>
   bool waitForResponse(const detail::ClientResponseHolder& resp, T1& t1, T&... t)
   {
      assert(resp.r_);
      assert(!running_);
      
      char* data = nullptr;
      size_t len = 0;
      
      int rc = loopUntil(resp.sequence_nr_, &data, &len);
      std::unique_ptr<char> raii(data);
      
      if (rc == 0)
      {
         ClientResponse<T1, T...>* r = safe_cast<ClientResponse<T1, T...>*>(resp.r_);
         assert(r);
         
         detail::Deserializer d(data, len);
         std::tuple<T1, T...> tup;
         d >> tup;
         
         assign<0>(tup, t1, t...);
      }
      
      return rc == 0;
   }

   int loopUntil(uint32_t sequence_nr = INVALID_SEQUENCE_NR, char** argData = nullptr, size_t* argLen = 0, unsigned int timeoutMs = 100);  ///< FIXME timeout must be somehow dynamic -> til next time_point 
   
   inline
   int once(unsigned int timeoutMs = 500)
   {
      char* data = nullptr;
      return once_(INVALID_SEQUENCE_NR, &data, 0, timeoutMs);
   }

   std::string fullQualifiedName(const char* ifname, const char* rolename);
   
   
private:

   void checkPendings(uint32_t current_sequence_number);
   
   std::chrono::steady_clock::time_point dueTime() const
   {
      return std::chrono::steady_clock::now() + request_timeout_;
   }

   void serviceReady(StubBase* stub, const std::string& fullName, const std::string& location);
   
   int accept_socket(int acceptor);
   
   int once_(uint32_t sequence_nr, char** argData, size_t* argLen, unsigned int timeoutMs);
   
public:
   
   int run();
   
   bool isSignalRegistered(ClientSignalBase& sigbase) const;
   
   void clearSlot(int idx);
   
   inline
   void stop()
   {
      running_ = false;
   }
   
   inline
   bool isRunning() const
   {
      return running_;
   }
   
   void* getSessionData(uint32_t sessionid);
   
   inline
   void registerSession(uint32_t fd, uint32_t sessionid, void* data, void(*destructor)(void*))
   {
      sessions_[sessionid] = SessionData(fd, data, destructor);
   }
   
   
private:

   void registerAtBroker(const std::string& service, const std::string& endpoint);

   bool connect(StubBase& stub, bool blockUntilResponse = false, const char* location = 0);

   
   //registered servers
   servermap_type servers_;
   servermapid_type servers_by_id_;
   std::map<uint32_t/*=sequencenr*/, std::tuple<StubBase*, std::chrono::steady_clock::time_point/*=duetime*/>> pending_interface_resolves_;
   
   uint32_t nextid_;
   volatile bool running_;   // FIXME better use atomic instead of volatile
   
   pollfd fds_[32];
   
   typedef int(Dispatcher::*acceptfunction_type)(int);
   acceptfunction_type af_[32];   ///< may have different kind of sockets here (tcp or unix)
   
   // registered clients
   clientmap_type clients_;
   
   uint32_t sequence_;
   
   std::map<uint32_t/*=sequencenr*/, 
     std::tuple<detail::Parented*, ClientResponseBase*, std::chrono::steady_clock::time_point/*=duetime*/>> pendings_;
   
   outstanding_signalregistrations_type outstanding_sig_registrs_;
   sighandlers_type sighandlers_;   
   serversignalers_type server_sighandlers_;
   
   socketsmap_type socks_;
   
   BrokerClient* broker_;
   std::vector<std::string> endpoints_;
   
   sessionmap_type sessions_;
   
   int selfpipe_[2];
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
   
   std::string name = fullQualifiedName(InterfaceNamer<typename ServerT::interface_type>::name(), serv.role_);
   
   assert(servers_.find(name) == servers_.end());
   std::cout << "Adding server for '" << name << "'" << std::endl;
   
   registerAtBroker(name, endpoints_.front());
   
   detail::ServerHolder<ServerT>* holder = new detail::ServerHolder<ServerT>(serv);
   servers_[name] = holder;
   servers_by_id_[generateId()] = holder;
   
   serv.disp_ = this;
}


}   // namespace ipc
   
}   // namespace simppl


#endif   // SIMPPL_DISPATCHER_H
