#ifndef IPC2_H
#define IPC2_H

#include <iostream>
#include <map>
#include <set>
#include <cassert>
#include <vector>
#include <algorithm>
#include <pthread.h>
#include <signal.h>

#include <functional>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdio>
#include <errno.h>
#include <stdint.h>
#include <memory>

#include "if.h"
#include "calltraits.h"
#include "noninstantiable.h"

#include "detail/serialization.h"

#define INVALID_SEQUENCE_NR 0xFFFFFFFF
#define INVALID_SERVER_ID 0u
#define INVALID_SESSION_ID 0u

#ifdef NDEBUG
#   define safe_cast static_cast
#else
#   define safe_cast dynamic_cast
#endif

using namespace std::placeholders;

// forward decls
template<typename IfaceT>
struct InterfaceNamer;

template<typename ServerT>
struct isServer;

struct BrokerClient;
struct Dispatcher;

#ifdef SIMPPL_HAVE_VALIDATION
#   include "detail/validation.h"
#else

template<typename... T>
struct isValidType
{
   enum { value = true };
};

#endif

// ------------------------------------------------------------------------------------


/// Base class for all IPC related exceptions.
struct IPCException : std::exception
{
   inline
   IPCException()
    : sequence_nr_(INVALID_SEQUENCE_NR)
   {
      // NOOP
   }

   explicit inline
   IPCException(uint32_t sequence_nr)
    : sequence_nr_(sequence_nr)
   {
      // NOOP
   }
   
   inline
   uint32_t sequenceNr() const
   {
      return sequence_nr_;
   }
   
private:
   
   uint32_t sequence_nr_;
};


// FIXME add a namespace simppl to everything in here
// FIXME move private stuff to detail namespace


/// blocking calls will throw, eventloop driven approach will call separate handlers
struct RuntimeError : IPCException
{
   explicit inline
   RuntimeError(int error)
    : IPCException()
    , error_(error)
    , message_(nullmsg_)
   {
      assert(error);   // zero is reserved value for "no error"
   }
   
   inline
   RuntimeError(int error, const char* message)
    : IPCException()
    , error_(error)
    , message_(message ? new char[::strlen(message)+1] : nullmsg_)
   {
      assert(error);
      
      if (message)
         ::strcpy(message_, message);
   }
   
   ~RuntimeError() throw()
   {
      if (message_ != nullmsg_)
         delete[] message_;
   }
   
   const char* what() const throw()
   {
      return message_;
   }
   
   inline
   int error() const
   {
      return error_;
   }
   
private:
   
   friend struct Dispatcher;
   
   /// only for dispatcher
   inline
   RuntimeError(int error, const char* message, uint32_t sequence_nr)
    : IPCException(sequence_nr)
    , error_(error)
    , message_(message ? new char[::strlen(message)+1] : nullmsg_)
   {
      assert(error);
      
      if (message)
         ::strcpy(message_, message);
   }
   
   int error_;
   char* message_;
   char nullmsg_[1];
};


struct TransportError : IPCException
{
   explicit inline
   TransportError(int errno__, uint32_t sequence_nr)
    : IPCException(sequence_nr)
    , errno_(errno__)
   {
      buf_[0] = '\0';
   }
   
   inline
   int getErrno() const
   {
      return errno_;
   }
   
   const char* what() const throw()
   {
      if (!buf_[0])
      {
         char buf[128];
         strcpy(buf_, strerror_r(errno_, buf, sizeof(buf)));
      }
      
      return buf_;
   }
   
   int errno_;
   mutable char buf_[128];
};


struct CallState
{
   explicit inline
   CallState(uint32_t sequenceNr)
    : ex_()
    , sequence_nr_(sequenceNr)
   {
      // NOOP
   }
   
   explicit inline
   CallState(IPCException* ex)
    : ex_(ex)
    , sequence_nr_(INVALID_SEQUENCE_NR)
   {
      // NOOP
   }
   
   inline
   operator const void*() const
   {
      return ex_.get() ? 0 : this;
   }
   
   inline
   uint32_t sequenceNr() const
   {
      return ex_.get() ? ex_->sequenceNr() : sequence_nr_;
   }
   
   inline
   bool isTransportError() const
   {
      return dynamic_cast<const TransportError*>(ex_.get());
   }
   
   inline
   bool isRuntimeError() const
   {
      return dynamic_cast<const RuntimeError*>(ex_.get());
   }
   
   inline
   const char* const what() const
   {
      return ex_->what();
   }
   
   inline
   const IPCException& exception() const
   {
      return *ex_;
   }
   
   
private:
   
   std::unique_ptr<IPCException> ex_;
   uint32_t sequence_nr_;
};


// ------------------------------------------------------------------------------------


struct SignalBlocker 
{
   inline
   SignalBlocker()
   {
      sigset_t set;
      sigfillset(&set);
      
      pthread_sigmask(SIG_BLOCK, &set, &old_);
   }
   
   inline
   ~SignalBlocker()
   {
      pthread_sigmask(SIG_SETMASK, &old_, 0);
   }
   
   sigset_t old_;
};


inline
std::string fullQualifiedName(const char* ifname, const char* rolename)
{
   std::string ret(ifname);
   ret += "::";
   ret += rolename;
   
   return ret;
}


template<typename StubT>
inline
std::string fullQualifiedName(const StubT& stub)
{
   std::string ret(stub.iface());
   ret += "::";
   ret += stub.role() ;
   
   return ret;
}


inline
const char* fullQualifiedName(char* buf, const char* ifname, const char* rolename)
{
   sprintf(buf, "%s::%s", ifname, rolename);
   return buf;
}


// ------------------------------------------------------------------------------------


// FIXME use calltraits or rvalue references here

template<int N, typename TupleT>
struct FunctionCaller
{
   template<typename FunctorT>
   static inline
   void eval(FunctorT& f, const TupleT& tuple)
   {
      FunctionCaller<N, TupleT>::template eval_intern(f, tuple);
   }
   
   template<typename FunctorT>
   static inline
   void eval_cs(FunctorT& f, const CallState& cs, const TupleT& tuple)
   {
      FunctionCaller<N, TupleT>::template eval_intern_cs(f, cs, tuple);
   }
   
   template<typename FunctorT, typename... T>
   static inline
   void eval_intern(FunctorT& f, const TupleT& tuple, const T&... t)
   {
      FunctionCaller<N+1 == std::tuple_size<TupleT>::value ? -1 : N+1, TupleT>::template eval_intern(f, tuple, t..., std::get<N>(tuple));
   }
   
   template<typename FunctorT, typename... T>
   static inline
   void eval_intern_cs(FunctorT& f, const CallState& cs, const TupleT& tuple, const T&... t)
   {
      FunctionCaller<N+1 == std::tuple_size<TupleT>::value ? -1 : N+1, TupleT>::template eval_intern_cs(f, cs, tuple, t..., std::get<N>(tuple));
   }
};

template<typename TupleT>
struct FunctionCaller<-1, TupleT>
{
   template<typename FunctorT, typename... T>
   static inline
   void eval_intern(FunctorT& f, const TupleT& /*tuple*/, const T&... t)
   {
      f(t...);
   }
   
   template<typename FunctorT, typename... T>
   static inline
   void eval_intern_cs(FunctorT& f, const CallState& cs, const TupleT& /*tuple*/, const T&... t)
   {
      f(cs, t...);
   }
};


template<typename... T>
struct DeserializeAndCall
{
   template<typename FunctorT>
   static inline
   void eval(Deserializer& d, FunctorT& f)
   {
      std::tuple<T...> tuple;
      d >> tuple;
      
      FunctionCaller<0, std::tuple<T...>>::template eval(f, tuple);
   }
   
   template<typename FunctorT>
   static inline
   void evalResponse(Deserializer& d, FunctorT& f, const CallState& cs)
   {
      std::tuple<T...> tuple;
      
      if (cs)
         d >> tuple;
      
      FunctionCaller<0, std::tuple<T...>>::template eval_cs(f, cs, tuple);
   }
};


struct DeserializeAndCall0 : NonInstantiable
{
   template<typename FunctorT>
   static inline
   void eval(Deserializer& /*d*/, FunctorT& f)
   {
      f();
   }
   
   template<typename FunctorT>
   static inline
   void evalResponse(Deserializer& /*d*/, FunctorT& f, const CallState& cs)
   {
      f(cs);
   }
};


template<typename... T>
struct GetCaller
{
   typedef typename if_<sizeof...(T) == 0, DeserializeAndCall0, DeserializeAndCall<T...>>::type type;
};


// -----------------------------------------------------------------------------


struct ServerRequestBase
{
   friend struct ServerRequestBaseSetter;
   
   virtual void eval(const void* payload, size_t length) = 0;

   inline
   bool hasResponse() const
   {
      return hasResponse_;
   }
   
protected:

   inline 
   ServerRequestBase()
    : hasResponse_(false)
   {
      // NOOP
   }
   
   inline
   ~ServerRequestBase() 
   {
      // NOOP
   }
   
   bool hasResponse_;
};


// move to namespace detail
struct ServerRequestBaseSetter
{
   template<typename T>
   static inline
   void setHasResponse(T& t)
   {
      t.hasResponse_ = true;
   }
};


struct ServerResponseBase
{
   std::set<ServerRequestBase*> allowedRequests_;
   
protected:
   
   inline
   ~ServerResponseBase()
   {
      // NOOP
   }
};


// -----------------------------------------------------------------------------


#define FRAME_MAGIC                            0xAABBCCDDu
#define FRAME_TYPE_REQUEST                     0x1u
#define FRAME_TYPE_RESPONSE                    0x2u
#define FRAME_TYPE_RESOLVE_INTERFACE           0x3u
#define FRAME_TYPE_RESOLVE_RESPONSE_INTERFACE  0x4u
#define FRAME_TYPE_REGISTER_SIGNAL             0x5u
#define FRAME_TYPE_UNREGISTER_SIGNAL           0x6u
#define FRAME_TYPE_SIGNAL                      0x7u
#define FRAME_TYPE_REGISTER_SIGNAL_RESPONSE    0x8u
#define FRAME_TYPE_TRANSPORT_ERROR             0x9u


struct FrameHeader
{
   inline
   FrameHeader()
    : magic_(0)
    , type_(0)
    , flags_(0)
    , payloadsize_(0)
    , sequence_nr_(0)
   {
      // NOOP
   }
   
   inline
   FrameHeader(uint32_t type)
    : magic_(FRAME_MAGIC)
    , type_(type)
    , flags_(0)
    , payloadsize_(0)
    , sequence_nr_(0)
   {
      // NOOP
   }
   
   inline
   operator const void*() const
   {
      return magic_ == FRAME_MAGIC ? this : 0;
   }
   
   uint32_t magic_;
   
   uint16_t type_;   
   uint16_t flags_;
   
   uint32_t payloadsize_;
   uint32_t sequence_nr_;
};


/// send interface::role name for service identification -> return will have an int32_t value which can
/// be used later on.
struct InterfaceResolveFrame : FrameHeader
{
   inline
   InterfaceResolveFrame()
    : FrameHeader()
   {
      // NOOP
   }
   
   inline
   InterfaceResolveFrame(int)
    : FrameHeader(FRAME_TYPE_RESOLVE_INTERFACE)
   {
      // NOOP
   }
};


struct InterfaceResolveResponseFrame : FrameHeader
{
   inline
   InterfaceResolveResponseFrame()
    : FrameHeader()
    , id_(INVALID_SERVER_ID)
    , sessionid_(INVALID_SESSION_ID)
   {
      // NOOP
   }
   
   inline
   InterfaceResolveResponseFrame(uint32_t id, uint32_t sessionid)
    : FrameHeader(FRAME_TYPE_RESOLVE_RESPONSE_INTERFACE)
    , id_(id)
    , sessionid_(sessionid)
   {
      // NOOP
   }
   
   uint32_t id_;         // invalid id is INVALID_SERVER_ID
   uint32_t sessionid_;
};


struct RequestFrame : FrameHeader
{
   inline
   RequestFrame()
    : FrameHeader()
    , serverid_(INVALID_SERVER_ID)
    , sessionid_(INVALID_SESSION_ID)
    , func_(0)
    , padding_(0)
   {
      // NOOP
   }
   
   inline
   RequestFrame(uint32_t serverid, uint32_t func, uint32_t sessionid)
    : FrameHeader(FRAME_TYPE_REQUEST)
    , serverid_(serverid)
    , sessionid_(sessionid)
    , func_(func)
    , padding_(0)
   {
      // NOOP
   }
   
   uint32_t serverid_;
   uint32_t sessionid_;
   uint32_t func_;
   uint32_t padding_;
};


struct ResponseFrame : FrameHeader
{
   inline
   ResponseFrame()
    : FrameHeader()
    , result_(0)
    , padding_(0)
   {
      // NOOP
   }
   
   inline
   ResponseFrame(int32_t result)
    : FrameHeader(FRAME_TYPE_RESPONSE)
    , result_(result)
    , padding_(0)
   {
      // NOOP
   }
   
   int32_t result_;     // 0 = ok result, any other value: exceptional result, exception text may be added as payload
   int32_t padding_;
};


struct RegisterSignalFrame : FrameHeader
{
   inline
   RegisterSignalFrame()
    : FrameHeader()
    , serverid_(0)
    , sig_(0)
    , id_(INVALID_SERVER_ID)
   {
      // NOOP
   }
   
   inline
   RegisterSignalFrame(uint32_t serverid, uint32_t sig, uint32_t clientsid)
    : FrameHeader(FRAME_TYPE_REGISTER_SIGNAL)
    , serverid_(serverid)
    , sig_(sig)
    , id_(clientsid)
   {
      // NOOP
   }
   
   uint32_t serverid_;
   uint32_t sig_;
   uint32_t id_;    ///< dispatcher-unique identifier ("cookie") for handler lookup on client side
};


struct UnregisterSignalFrame : FrameHeader
{
   inline
   UnregisterSignalFrame()
    : FrameHeader()
   {
      memset(this, 0, sizeof(*this));
   }
   
   inline
   UnregisterSignalFrame(uint32_t registrationid)
    : FrameHeader(FRAME_TYPE_UNREGISTER_SIGNAL)
    , registrationid_(registrationid)
   {
   }
   
   uint32_t registrationid_;
   uint32_t padding_;
};


struct SignalEmitFrame : FrameHeader
{
   inline
   SignalEmitFrame()
    : FrameHeader()
    , id_(0)
    , padding_(0)
   {
      // NOOP
   }
   
   inline
   SignalEmitFrame(uint32_t id)
    : FrameHeader(FRAME_TYPE_SIGNAL)
    , id_(id)
    , padding_(0)
   {
      // NOOP
   }
   
   uint32_t id_;
   uint32_t padding_;
};


struct SignalResponseFrame : FrameHeader
{
   inline
   SignalResponseFrame()
    : FrameHeader()
    , registrationid_(0)
    , id_(0)
   {
      // NOOP
   }
   
   inline
   SignalResponseFrame(uint32_t registrationid, uint32_t id)
    : FrameHeader(FRAME_TYPE_REGISTER_SIGNAL_RESPONSE)
    , registrationid_(registrationid)
    , id_(id)
   {
      // NOOP
   }
   
   uint32_t registrationid_;  ///< server provided registration id for deregistration purpose
   uint32_t id_;              ///< client provided "cookie" id
};


struct TransportErrorFrame : FrameHeader
{
   inline
   TransportErrorFrame()
    : handler_(0)
    , err_(0)
   {
      // NOOP
   }
   
   inline
   TransportErrorFrame(void* handler, void* err)
    : FrameHeader(FRAME_TYPE_TRANSPORT_ERROR)
    , handler_(handler)
    , err_(err)
   {
      // NOOP
   }
   
   void* handler_;   ///< internal pointer to ClientResponseBase 
   void* err_;       ///< internal pointer to TransportError
};


static
size_t headersize[] {
   0,
   sizeof(RequestFrame), 
   sizeof(ResponseFrame),    
   sizeof(InterfaceResolveFrame),
   sizeof(InterfaceResolveResponseFrame),
   sizeof(RegisterSignalFrame),
   sizeof(UnregisterSignalFrame),
   sizeof(SignalEmitFrame),
   sizeof(SignalResponseFrame),
   sizeof(TransportErrorFrame)
};


// -------------------------------------------------------------------------------------------

   
template<typename FrameT>
bool genericSend(int fd, const FrameT& f, const void* payload)
{
   iovec iov[2] = { { const_cast<FrameT*>(&f), sizeof(FrameT) }, { const_cast<void*>(payload), f.payloadsize_ } };
   
   msghdr msg;
   ::memset(&msg, 0, sizeof(msg));
   msg.msg_iov = iov;
   msg.msg_iovlen = payload ? 2 : 1;
   
   if (::sendmsg(fd, &msg, MSG_NOSIGNAL) != sizeof(FrameT) + f.payloadsize_)
   {
      ::perror("sendmsg failed");
      return false;
   }
   
   return true;
}


// -------------------------------------------------------------------------------------------


struct ServerResponseHolder 
{
   ServerResponseHolder(Serializer& s, ServerResponseBase& responder)
    : size_(s.size())
    , payload_(s.release())
    , responder_(&responder)
   {
      // NOOP
   }
   
   /// move semantics
   ServerResponseHolder(const ServerResponseHolder& rhs)
    : payload_(rhs.payload_)
    , size_(rhs.size_)
    , responder_(rhs.responder_)
   {
      rhs.payload_ = 0;
      rhs.size_ = 0;
   }
   
   ~ServerResponseHolder()
   {
      Serializer::free(payload_);
   }
   
   /// move semantics
   ServerResponseHolder& operator=(const ServerResponseHolder& rhs)
   {
      if (this != &rhs)
      {
         payload_ = rhs.payload_;
         size_ = rhs.size_;
         responder_ = rhs.responder_;
   
         rhs.payload_ = 0;
         rhs.size_ = 0;
         rhs.responder_ = 0;
      }
      
      return *this;
   }
   
   mutable size_t size_;
   mutable void* payload_;
   mutable ServerResponseBase* responder_;
};


struct Parented
{
   friend struct StubBase;
   friend struct Dispatcher;
   
protected:
 
   inline
   Parented()
    : parent_(0)
   {
      // NOOP
   }
   
   inline
   void reparent(void* parent)
   {
      assert(parent_);
      parent_ = parent;
   }
   
   template<typename ParentT>
   inline
   ParentT* parent()
   {
      assert(parent_);
      return (ParentT*)parent_;
   }
   
   inline
   ~Parented()
   {
      // NOOP
   }
   
   void* parent_;
};


// forward decls
struct ClientResponseBase;
struct ClientSignalBase;


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
   
   void reparent(Parented* child)
   {
      child->parent_ = this;
   }
   
public:
   
   std::function<void()> connected;
   
   StubBase(const char* iface, const char* role, const char* boundname)
    : iface_(iface) 
    , role_(role)
    , id_(INVALID_SERVER_ID)
    , disp_(0)
    , fd_(-1)
    , current_sessionid_(INVALID_SESSION_ID)
   {
      assert(iface);
      assert(role);
      assert(boundname && strlen(boundname) < sizeof(boundname_));
      strcpy(boundname_, boundname);
   }
   
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
   
   inline
   Dispatcher& disp()
   {
      assert(disp_);
      return *disp_;
   }
   
   inline
   bool isConnected() const
   {
      return id_ != INVALID_SERVER_ID && fd_ != -1;
   }
   
protected:
   
   uint32_t sendRequest(Parented& requestor, ClientResponseBase* handler, uint32_t requestid, const Serializer& s);
   
   void sendSignalRegistration(ClientSignalBase& sigbase);
   void sendSignalUnregistration(ClientSignalBase& sigbase);
   
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


// -------------------------------------------------------------------------------------------


struct ClientResponseBase
{
   virtual void eval(const CallState& state, const void* payload, size_t len) = 0;
   
protected:

   inline
   ~ClientResponseBase()
   {
      // NOOP
   }
};


// ---------------------------------------------------------------------------------------


struct ClientSignalBase : Parented
{
   virtual void eval(const void* data, size_t len) = 0;
   
   ClientSignalBase(uint32_t id)
    : id_(id)
   {
      // NOOP
   }
   
   uint32_t id() const   // make this part of a generic Identifiable baseclass
   {
      return id_;
   }
   
protected:
   
   inline
   ~ClientSignalBase()
   {
      // NOOP
   }
   
   uint32_t id_;   
};


template<typename... T>
struct ClientSignal : ClientSignalBase
{
   static_assert(isValidType<T...>::value, "invalid type in interface");
   
   typedef std::function<void(typename CallTraits<T>::param_type...)> function_type;
      
   inline
   ClientSignal(uint32_t id, std::vector<Parented*>& parent)
    : ClientSignalBase(id)
   {
      parent.push_back(this);
   }
      
   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      assert(!f_);
      f_ = func;
   }
   
   /// send registration to the server - only attach after the interface is connected.
   inline
   ClientSignal& attach()
   {
      parent<StubBase>()->sendSignalRegistration(*this);
      return *this;
   }
   
   /// send de-registration to the server - only attach after the interface is connected.
   inline
   ClientSignal& detach()
   {
      parent<StubBase>()->sendSignalUnregistration(*this);
      return *this;
   }
   
   void eval(const void* payload, size_t length)
   {
      if (f_)
      {
         Deserializer d(payload, length);
         GetCaller<T...>::type::template eval(d, f_);
      }
      else
         std::cerr << "No appropriate handler registered for signal " << id_ << " with payload size=" << length << std::endl;
   }
   
   function_type f_;
};


template<typename... T, typename FuncT>
inline
void operator>>(ClientSignal<T...>& sig, const FuncT& func)
{
   sig.handledBy(func);
}


struct SignalRecipient
{
   inline
   SignalRecipient(int fd, uint32_t clientsid)
    : fd_(fd)
    , clientsid_(clientsid)
   {
      // NOOP
   }
   
   // Never call directly, for std::map only
   inline
   SignalRecipient()
    : fd_(-1)
    , clientsid_(0)
   {
      // NOOP
   }
   
   int fd_;
   uint32_t clientsid_;   ///< client side id of recipient (for routing on client side)
};


struct SignalSender
{
   inline
   SignalSender(const void* data, size_t len)
    : data_(data)
    , len_(len)
   {
      // NOOP
   }
   
   inline
   void operator()(const std::pair<uint32_t, SignalRecipient>& info)
   {
      operator()(info.second);
   }
   
   inline
   void operator()(const SignalRecipient& info)
   {
      SignalEmitFrame f(info.clientsid_);
      f.payloadsize_ = len_;
      
      // FIXME need to remove socket on EPIPE 
      genericSend(info.fd_, f, data_);
   }
   
   const void* data_;
   size_t len_;
};


// -------------------------------------------------------------------------------


// partial update mode for attributes
enum How
{
   None = -1,
   Full = 0,
   Replace,   ///< replace and/or append (depending on given index)
   Remove,
   Insert,
};


template<typename VectorT>
struct ClientVectorAttributeUpdate
{   
   inline   
   ClientVectorAttributeUpdate()
    : how_(None)
    , where_(0)
    , len_(0)
   {
      // NOOP
   }
   
   VectorT data_;
   How how_;
   uint32_t where_;
   uint32_t len_;
};


template<typename VectorT>
Deserializer& operator>>(Deserializer& istream, ClientVectorAttributeUpdate<VectorT>& updt)
{
   istream >> (uint32_t&)updt.how_;
   istream >> updt.where_;
   istream >> updt.len_;
   
   if (updt.how_ != Remove)
   {
      updt.data_.resize(updt.len_);
      
      for(int i=0; i<updt.len_; ++i)
      {
         istream >> updt.data_[i];
      }
   }
   
   return istream;
}


template<typename VectorT>
struct ServerVectorAttributeUpdate
{
   ServerVectorAttributeUpdate(const VectorT& vec, How how = Full, uint32_t from = 0, uint32_t len = 0)
    : data_(vec)
    , how_(how)
    , where_(from)
    , len_(how == Full ? vec.size() : len)
   {
      // NOOP
   }
   
   const VectorT& data_;
   How how_;
   uint32_t where_;
   uint32_t len_;
};


template<typename VectorT>
Serializer& operator<<(Serializer& ostream, const ServerVectorAttributeUpdate<VectorT>& updt)
{
   ostream << (uint32_t)updt.how_;
   ostream << updt.where_;
   ostream << updt.len_;
   
   if (updt.how_ != Remove)
   {
      for(int i=updt.where_; i<updt.where_+updt.len_; ++i)
      {
         ostream << updt.data_[i];
      }
   }
   
   return ostream;
}


template<typename VectorT>
struct isValidType<ServerVectorAttributeUpdate<VectorT>>
{
   enum { value = true };
};

template<typename VectorT>
struct isValidType<ClientVectorAttributeUpdate<VectorT>>
{
   enum { value = true };
};


// --------------------------------------------------------------------------------------


template<typename T>
struct is_vector
{
   enum { value = false };
};


template<typename T>
struct is_vector<std::vector<T> >
{
   enum { value = true };
};


struct OnChange
{
   template<typename T>
   static inline 
   bool eval(T& lhs, const T& rhs)
   {
      return lhs != rhs;
   }
};


struct Always
{
   template<typename T>
   static inline 
   bool eval(const T&, const T&)
   {
      return true;
   }
};


struct Committed
{
   template<typename T>
   static inline 
   bool eval(const T&, const T&)
   {
      return false;
   }
};


template<typename DataT, typename EmitPolicyT>
struct ClientAttribute
{
   static_assert(isValidType<DataT>::value, "invalid type in interface");
   
   typedef typename CallTraits<DataT>::param_type arg_type;
   
   typedef typename if_<is_vector<DataT>::value, 
      std::function<void(arg_type, How, uint32_t, uint32_t)>, 
      std::function<void(arg_type)> >::type function_type;
   
   inline
   ClientAttribute(uint32_t id, std::vector<Parented*>& parent)
    : signal_(id, parent)
    , data_()
   {
      // NOOP
   }
   
   inline
   const DataT& value() const
   {
      return data_;
   }
   
   
   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      assert(!f_);
      f_ = func;
   }
   
   /// only call this after the server is connected.
   inline
   ClientAttribute& attach()
   {
      signal_.handledBy(std::bind(&ClientAttribute<DataT, EmitPolicyT>::valueChanged, this, _1));
      
      (void)signal_.attach();
      return *this;
   }
   
   /// only call this after the server is connected.
   inline
   ClientAttribute& detach()
   {
      (void)signal_.detach();
      return *this;
   }
   
private:

   /// vector argument with partial update support   
   typedef typename if_<is_vector<DataT>::value, ClientVectorAttributeUpdate<DataT>, DataT>::type signal_arg_type;
   typedef ClientSignal<signal_arg_type> signal_type;
   
   void setAndCall(const ClientVectorAttributeUpdate<DataT>& varg)
   {
      switch (varg.how_)
      {
      case Full:
         data_ = varg.data_;
         break;

      case Remove:
         data_.erase(data_.begin()+varg.where_, data_.begin() + varg.where_ + varg.len_);
         break;

      case Insert:
         data_.insert(data_.begin()+varg.where_, varg.data_.begin(), varg.data_.end());
         break;

      case Replace:
         assert(varg.data_.size() == varg.len_);

         for(int i=0; i<varg.len_; ++i)
         {
            if (i+varg.where_ < data_.size())
            {
               data_[i+varg.where_] = varg.data_[i];
            }
            else            
               data_.push_back(varg.data_[i]);            
         }
         break;

      default:
         // NOOP
         break;
      }

      if (f_)
         f_(data_, varg.how_, varg.where_, varg.len_);
   }

   /// normal argument - will not work with vectors and partial update support
   inline
   void setAndCall(arg_type arg)
   {
      data_ = arg;

      if (f_)
         f_(data_);
   }

   void valueChanged(typename CallTraits<signal_arg_type>::param_type arg)
   {      
      setAndCall(arg);
   }
      
   signal_type signal_;
   DataT data_;
   
   function_type f_;
};


template<typename DataT, typename EmitPolicyT, typename FuncT>
inline
void operator>>(ClientAttribute<DataT,EmitPolicyT>& attr, const FuncT& func)
{
   attr.handledBy(func);
}


// --------------------------------------------------------------------------------------


struct ServerSignalBase
{   
   inline
   void sendSignal(const Serializer& s)
   {
      std::for_each(recipients_.begin(), recipients_.end(), SignalSender(s.data(), s.size()));
   }
   
   inline
   void addRecipient(int fd, uint32_t registrationid, uint32_t clientsid)
   {
      SignalRecipient rcpt(fd, clientsid);
      recipients_[registrationid] = rcpt;
   }

   inline
   void removeRecipient(uint32_t registrationid)
   {
      recipients_.erase(registrationid);
   }
   
   virtual void onAttach(uint32_t /*registrationid*/)
   {
      // NOOP
   }
   
   /// remove all recipients which may emit to the file descriptor 'fd'
   inline
   void removeAllWithFd(int fd)
   {
      // algorithm could be better, mmmhh?!
      for(std::map<uint32_t, SignalRecipient>::iterator iter = recipients_.begin(); iter != recipients_.end(); )
      {
         if (iter->second.fd_ == fd)
         {
            recipients_.erase(iter);
            iter = recipients_.begin();
         }
         else
            ++iter;
      }
   }
   
protected:
   
   inline
   ~ServerSignalBase()
   {
      // NOOP
   }
   
   std::map<uint32_t/*registrationid*/, SignalRecipient> recipients_;
};


template<typename... T>
struct ServerSignal : ServerSignalBase
{
   static_assert(isValidType<T...>::value, "invalid type in interface");
   
   inline
   ServerSignal(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
   {
      _signals[id] = this;
   }
   
   inline
   void emit(typename CallTraits<T>::param_type... args)
   {
      Serializer s;
      sendSignal(serialize(s, args...));
   }
   
   
protected:
   
   inline
   void emitWithId(uint32_t registrationid, T... t)
   {
      Serializer s;
      serialize(s, t...);

      SignalSender(s.data(), s.size())(ServerSignalBase::recipients_[registrationid]);
   }
};


template<typename DataT>
struct BaseAttribute 
 : ServerSignal<typename if_<is_vector<DataT>::value, ServerVectorAttributeUpdate<DataT>, DataT>::type> 
{
   inline
   BaseAttribute(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
    : ServerSignal<typename if_<is_vector<DataT>::value, ServerVectorAttributeUpdate<DataT>, DataT>::type>(id, _signals)
   {
      // NOOP
   }
    
   inline
   const DataT& value() const
   {
      return t_;
   }
 
protected:
      
   DataT t_;
};


// horrible hack to remove the constness of the stl iterator for vector,
// maybe there are better solutions to achieve this?
template<typename IteratorT, typename ContainerT>
inline
__gnu_cxx::__normal_iterator<typename remove_const<IteratorT>::type, ContainerT>
unconst(__gnu_cxx::__normal_iterator<IteratorT, ContainerT>& iter)
{
   return __gnu_cxx::__normal_iterator<typename remove_const<IteratorT>::type, ContainerT>(
      const_cast<typename remove_const<IteratorT>::type>(iter.base()));
}


// forward decl
template<typename T, typename EmitPolicyT>
struct VectorAttributeMixin;


template<typename T, typename EmitPolicyT>
struct VectorValue
{
   inline
   VectorValue(VectorAttributeMixin<std::vector<T>, EmitPolicyT>& mixin, typename std::vector<T>::iterator iter)
    : mixin_(mixin)
    , iter_(iter)
   {
      // NOOP
   }
   
   T& operator=(const T& t);
   
   VectorAttributeMixin<std::vector<T>, EmitPolicyT>& mixin_;
   typename std::vector<T>::iterator iter_;
};
   

template<typename T, typename EmitPolicyT>
struct VectorAttributeMixin : BaseAttribute<T>
{
public:
   
   inline
   VectorAttributeMixin(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
    : BaseAttribute<T>(id, _signals)
   {
      // NOOP
   }
   
   typedef typename T::pointer pointer;
   typedef typename T::iterator iterator;
   typedef typename T::const_iterator const_iterator;
   typedef typename T::value_type value_type;
   
   template<typename IteratorT>
   inline
   void insert(const_iterator where, IteratorT from, IteratorT to)
   {
      insert(unconst(where), from, to, bool_<std::is_integral<IteratorT>::value>());
   }
   
   inline
   void insert(const_iterator where, const value_type& value)
   {
      this->t_.insert(unconst(where), value);
      emit(ServerVectorAttributeUpdate<T>(this->t_, Insert, where - this->t_.begin(), 1));
   }
   
   inline
   void erase(const_iterator where)
   {
      if (EmitPolicyT::eval(where, this->t_.end()))
      {
         this->t_.erase(iterator(const_cast<pointer>(where.base())));
         emit(ServerVectorAttributeUpdate<T>(this->t_, Remove, where-this->t_.begin(), 1));
      }
   }
   
   inline
   void erase(const_iterator from, const_iterator to)
   {
      if (EmitPolicyT::eval(from, this->t_.end()))
      {
         this->t_.erase(unconst(from), unconst(to));
         emit(ServerVectorAttributeUpdate<T>(this->t_, Remove, from-begin(), to-from));
      }
   }
   
   inline
   void clear()
   {
      erase(begin(), end());
   }
   
   inline
   bool empty() const
   {
      return this->t_.empty();
   }
   
   inline
   void push_back(const value_type& val)
   {
      this->t_.push_back(val);
      emit(ServerVectorAttributeUpdate<T>(this->t_, Replace, this->t_.size()-1, 1));
   }
   
   inline
   void pop_back()
   {
      this->t_.pop_back();
      emit(ServerVectorAttributeUpdate<T>(this->t_, Remove, this->t_.size(), 1));
   }
   
   inline
   size_t size() const
   {
      return this->t_.size();
   }
   
   inline
   const_iterator begin() const
   {
      return this->t_.begin();
   }
   
   inline
   const_iterator end() const
   {
      return this->t_.end();
   }
   
   inline
   VectorValue<value_type, EmitPolicyT> operator[](size_t idx)
   {
      return VectorValue<value_type, EmitPolicyT>(*this, this->t_.begin()+idx);
   }
   
   template<typename IteratorT>
   inline
   void replace(const_iterator where, IteratorT from, IteratorT to)
   {
      bool doEmit = false;
      
      size_t len = std::distance(from, to);
      for(iterator iter = unconst(where); iter != this->t_.end() && from != to; ++iter, ++from)
      {
         this->t_.replace(unconst(where), *from);
         
         if (EmitPolicyT::eval(*where, *from))
            doEmit = true;
      }

      if (from != to)
      {
         doEmit = true;

         while(from != to)
         {
            this->t_.push_back(*from);
            ++from;
         }
      }
      
      if (EmitPolicyT::eval(doEmit, false))
         emit(ServerVectorAttributeUpdate<T>(this->t_, Replace, where-this->t_.begin(), len));
   }
   
   inline
   void replace(const_iterator where, const value_type& v)
   {
      *unconst(where) = v;
         
      if (EmitPolicyT::eval(*where, v))
         emit(ServerVectorAttributeUpdate<T>(this->t_, Replace, where-this->t_.begin(), 1));
   }
   
protected:
   
   inline
   void insert(const_iterator where, size_t count, const value_type& value, tTrueType)
   {
      this->t_.insert(unconst(where), count, value);
      emit(ServerVectorAttributeUpdate<T>(this->t_, Insert, where - this->t_.begin(), count));
   }
   
   template<typename IteratorT>
   inline
   void insert(const_iterator where, IteratorT from, IteratorT to, tFalseType)
   {
      this->t_.insert(unconst(where), from, to);
      emit(ServerVectorAttributeUpdate<T>(this->t_, Insert, where - this->t_.begin(), std::distance(from, to)));
   }
};


template<typename T, typename EmitPolicyT>
inline
T& VectorValue<T, EmitPolicyT>::operator=(const T& t)
{
   mixin_.replace(iter_, t);
   return *iter_;
}


/// a NOOP
template<typename EmitPolicyT, typename BaseT>
struct CommitMixin : BaseT
{
   inline
   CommitMixin(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
    : BaseT(id, _signals)
   {
      // NOOP
   }
};


/// add a commit function to actively transfer the attribute data
template<typename BaseT>
struct CommitMixin<Committed, BaseT> : BaseT
{
   inline
   CommitMixin(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
    : BaseT(id, _signals)
   {
      // NOOP
   }
   
   void commit()
   {
      this->emit(this->t_);
   }
};


template<typename DataT, typename EmitPolicyT>
struct ServerAttribute 
   : CommitMixin<EmitPolicyT, typename if_<is_vector<DataT>::value, VectorAttributeMixin<DataT, EmitPolicyT>, BaseAttribute<DataT> >::type> 
{
   static_assert(isValidType<DataT>::value, "invalid type in interface");
   
   typedef CommitMixin<EmitPolicyT, typename if_<is_vector<DataT>::value, VectorAttributeMixin<DataT, EmitPolicyT>, BaseAttribute<DataT> >::type>  baseclass;
   
   inline
   ServerAttribute(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
    : baseclass(id, _signals)
   {
      // NOOP
   }
   
   inline
   ServerAttribute& operator=(const DataT& data)
   {
      this->t_ = data;
      
      if (EmitPolicyT::eval(this->t_, data))
         emit(this->t_);
      
      return *this;
   }   
   
   void onAttach(uint32_t registrationid)
   {
      emitWithId(registrationid, this->t_);
   }
};


// ---------------------------------------------------------------------------------------


struct ClientResponseHolder
{
   inline
   ClientResponseHolder(ClientResponseBase* r, uint32_t sequence_nr)
    : r_(r)
    , sequence_nr_(sequence_nr)
   {
      // NOOP
   }
   
   ClientResponseBase* r_;
   uint32_t sequence_nr_;
};


template<typename... T>
struct ClientRequest : Parented
{
   static_assert(isValidType<T...>::value, "invalid_type_in_interface");
      
   inline
   ClientRequest(uint32_t id, std::vector<Parented*>& parent)
    : id_(id)
    , handler_(0)
   {
      parent.push_back(this);
   }
      
   inline
   ClientResponseHolder operator()(typename CallTraits<T>::param_type... t)
   {
      Serializer s; //FIXME (sizeof(typename remove_ref<T1>::type));
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(*this, handler_, id_, serialize(s, t...)));
   }

   ClientResponseBase* handler_;
   uint32_t id_;
};


// ----------------------------------------------------------------------------------------


template<typename... T>
struct ClientResponse : ClientResponseBase
{ 
   static_assert(isValidType<T...>::value, "invalid_type_in_interface");
   
   typedef std::function<void(const CallState&, typename CallTraits<T>::param_type...)> function_type;
   
   inline
   ClientResponse()
   {
      // NOOP
   }
   
   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      assert(!f_);
      f_ = func;
   }
   
   void eval(const CallState& cs, const void* payload, size_t length)
   {
      if (f_)
      {
         Deserializer d(payload, length);
         GetCaller<T...>::type::template evalResponse(d, f_, cs);
      }
      else
         std::cerr << "No appropriate handler registered for response with payload size=" << length << std::endl;
   }
   
   function_type f_;
};



template<typename FunctorT, typename... T>
inline 
ClientResponse<T...>& operator>> (ClientResponse<T...>& r, const FunctorT& f)
{
   r.handledBy(f);
   return r;
}


// -------------------------------------------------------------------------------


template<typename... T>
struct ServerRequest : ServerRequestBase
{
   static_assert(isValidType<T...>::value, "invalid_type_in_interface");
   
   typedef std::function<void(typename CallTraits<T>::param_type...)> function_type;
     
   inline
   ServerRequest(uint32_t id, std::map<uint32_t, ServerRequestBase*>& requests)
   {
      requests[id] = this;
   }
   
   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      assert(!f_);
      f_ = func;
   }
      
   void eval(const void* payload, size_t length)
   {
      if (f_)
      {
         Deserializer d(payload, length);
         GetCaller<T...>::type::template eval(d, f_);
      }
      else
         std::cerr << "No appropriate handler registered for request with payload size=" << length << std::endl;
   }
   
   function_type f_;
};


template<typename FunctorT, typename... T>
inline 
void operator>> (ServerRequest<T...>& r, const FunctorT& f)
{
   r.handledBy(f);
}


// ---------------------------------------------------------------------------------------------


template<typename... T>
struct ServerResponse : ServerResponseBase
{   
   static_assert(isValidType<T...>::value, "invalid_type_in_interface");
   
   inline
   ServerResponse()
   {
      // NOOP
   }
   
   inline
   ServerResponseHolder operator()(typename CallTraits<T>::param_type&... t)
   { 
      Serializer s;
      return ServerResponseHolder(serialize(s, std::forward<T...>(t...)), *this);
   }
};


struct ServerHolderBase
{
   virtual ~ServerHolderBase()
   {
      // NOOP
   }
   
   /// handle request
   virtual void eval(uint32_t funcid, uint32_t sequence_nr, uint32_t sessionid, int fd, const void* payload, size_t len) = 0;
   
   virtual ServerSignalBase* addSignalRecipient(uint32_t id, int fd, uint32_t registrationid, uint32_t clientsid) = 0;
};


template<typename SkeletonT>
struct ServerHolder : ServerHolderBase
{
   /// satisfy std::map only. Never call!
   inline
   ServerHolder()
    : handler_(0)
   {
      // NOOP
   }
   
   inline
   ServerHolder(SkeletonT& skel)
    : handler_(&skel)
   {
      // NOOP
   }
   
   /*virtual*/
   void eval(uint32_t funcid, uint32_t sequence_nr, uint32_t sessionid, int fd, const void* payload, size_t len)
   {
      handler_->handleRequest(funcid, sequence_nr, sessionid, fd, payload, len);
   }
   
   /*virtual*/
   ServerSignalBase* addSignalRecipient(uint32_t id, int fd, uint32_t registrationid, uint32_t clientsid)
   {
      ServerSignalBase* rc = nullptr;
      
      std::map<uint32_t, ServerSignalBase*>::iterator iter = handler_->signals_.find(id);
      if (iter != handler_->signals_.end())
      {
         iter->second->addRecipient(fd, registrationid, clientsid);
         rc = iter->second;
      }
      
      return rc;
   }
   
   SkeletonT* handler_;
};
   

/// session data holder
struct SessionData
{
   inline
   SessionData()
    : fd_(-1)
    , data_(0)
    , destructor_(0)
   {
      // NOOP
   }
   
   /// move semantics
   SessionData(const SessionData& rhs)
    : fd_(rhs.fd_)
    , data_(rhs.data_)
    , destructor_(rhs.destructor_)
   {
      SessionData* that = const_cast<SessionData*>(&rhs);
      that->fd_ = -1;
      that->data_ = 0;
      that->destructor_ = 0;
   }
   
   inline
   SessionData(int fd, void* data, void(*destructor)(void*))
    : fd_(fd)
    , data_(data)
    , destructor_(destructor)
   {
      // NOOP
   }
   
   /// move semantics
   SessionData& operator=(const SessionData& rhs)
   {
      if (this != &rhs)
      {
         fd_ = rhs.fd_;
         data_ = rhs.data_;
         destructor_ = rhs.destructor_;
         
         SessionData* that = const_cast<SessionData*>(&rhs);
         that->fd_ = -1;
         that->data_ = 0;
         that->destructor_ = 0;
      }
      
      return *this;
   }
   
   inline
   ~SessionData()
   {
      if (data_ && destructor_)
         (*destructor_)(data_);
   }
   
   int fd_;                     ///< associated file handle the session belongs to
   void* data_;                 ///< the session data pointer
   void(*destructor_)(void*);   ///< destructor function to use for destruction
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


static std::unique_ptr<char> NullUniquePtr;


struct Dispatcher
{
   friend struct StubBase;
   
   // resolve server name to id
   typedef std::map<std::string/*=server::role*/, ServerHolderBase*> servermap_type;  
   
   // signal registration and request resolution
   typedef std::map<uint32_t/*=serverid*/, ServerHolderBase*> servermapid_type;
   
   // all registered clients
   typedef std::multimap<std::string/*=server::role the client is connected to*/, StubBase*> clientmap_type;
   
   // all client side socket connections (good for multiplexing) FIXME need refcounting and reconnect strategy
   typedef std::map<std::string/*boundname*/, int/*=fd*/> socketsmap_type;
   
   // temporary maps, should always shrink again, maybe could drop entries on certain timeouts
   typedef std::map<uint32_t/*=sequencenr*/, std::tuple<Parented*, ClientResponseBase*> > outstanding_requests_type;
   typedef std::map<uint32_t/*=sequencenr*/, ClientSignalBase*> outstanding_signalregistrations_type;
   typedef std::map<uint32_t/*=sequencenr*/, StubBase*> outstanding_interface_resolves_type;
   
   // signal handling client- and server-side
   typedef std::map<uint32_t/*=clientside_id*/, ClientSignalBase*> sighandlers_type;
   typedef std::map<uint32_t/*=serverside_id*/, ServerSignalBase*> serversignalers_type;
   
   typedef std::map<uint32_t/*=sessionid*/, SessionData> sessionmap_type;
   
   static
   void makeInetAddress(struct sockaddr_in& addr, const char* endpoint)
   {
      assert(strlen(endpoint) < 64);
      
      char tmp[64];
      ::strcpy(tmp, endpoint);
      char* port = tmp + 4;
      while(*++port != ':');
      *port = '\0';
      ++port;
      //std::cout << "'" << tmp << " : '" << port << "'" << std::endl;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(::atoi(port));
      addr.sin_addr.s_addr = ::inet_addr(tmp + 4);
   }

   
   /// boundname may be 0 for handling clients without any servers
   Dispatcher(const char* boundname = nullptr)
    : running_(false)
    , sequence_(0)
    , nextid_(0)
    , broker_(0)
   {
      ::memset(fds_, 0, sizeof(fds_));
      ::memset(af_, 0, sizeof(af_));
      
      if (boundname)
      {
         assert(attach(boundname));
      }
      
      // can't use pipe since then the reading code would have to be changed...
      int rc = ::socketpair(PF_UNIX, SOCK_STREAM, 0, selfpipe_);
      assert(!rc);
      
      fds_[selfpipe_[0]].fd = selfpipe_[0];
      fds_[selfpipe_[0]].events = POLLIN;
   }
   
   /// allow clients to send requests from threads other the dispatcher is running in
   void enableThreadedClient()
   {
      // FIXME
   }
   
   /// allow dispatcher to talk to broker for registering new services and to
   /// wait for clients to attach services
   void enableBrokerage();
   
   virtual ~Dispatcher();
   
   virtual void socketConnected(int /*fd*/)
   {
      // NOOP
   }
   
   virtual void socketDisconnected(int /*fd*/)
   {
      // NOOP
   }
   
   /// attach multiple transport endpoints (e.g. tcp socket or datagram transport endpoint)
   /// e.g.   attach("unix:/server1")
   ///        attach("tcp:127.0.0.1:8888")
   bool attach(const char* endpoint)
   {
      bool rc = false;
      
      if (!strncmp(endpoint, "unix:", 5) && strlen(endpoint) > 5)
      {
         // unix acceptor
         ::mkdir("/tmp/dispatcher", 0777);
         
         int acceptor = ::socket(PF_UNIX, SOCK_STREAM, 0);
         
         struct sockaddr_un addr;
         addr.sun_family = AF_UNIX;
         sprintf(addr.sun_path, "/tmp/dispatcher/%s", endpoint + 5);
         
         ::unlink(addr.sun_path);         
         ::bind(acceptor, (struct sockaddr*)&addr, sizeof(addr));
         
         ::listen(acceptor, 16);
         
         fds_[acceptor].fd = acceptor;
         fds_[acceptor].events = POLLIN;
         
         af_[acceptor] = &Dispatcher::accept_socket;
         rc = true;
      }
      else if (!strncmp(endpoint, "tcp:", 4) && strlen(endpoint) > 4)
      {
         // tcp acceptor
         int acceptor = ::socket(PF_INET, SOCK_STREAM, 0);
       
         struct sockaddr_in addr;
         makeInetAddress(addr, endpoint);
         ::bind(acceptor, (struct sockaddr*)&addr, sizeof(addr));
         
         ::listen(acceptor, 16);
         
         fds_[acceptor].fd = acceptor;
         fds_[acceptor].events = POLLIN;
         
         af_[acceptor] = &Dispatcher::accept_socket;
         rc = true;
      }
      
      if (rc)
         endpoints_.push_back(endpoint);
         
      return rc;
   }
   
   
   template<typename ServerT>
   void addServer(ServerT& serv);
   
   inline
   uint32_t generateId()
   {
      return ++nextid_;
   }
   
   inline
   void addRequest(Parented& req, ClientResponseBase& resp, uint32_t sequence_nr)
   {
      outstandings_[sequence_nr] = std::tuple<Parented*, ClientResponseBase*>(&req, &resp);
   }
   
   inline
   bool addSignalRegistration(ClientSignalBase& s, uint32_t sequence_nr)
   {
      bool alreadyAttached = false;
      
      // FIXME check attached list here...
      
      for (outstanding_signalregistrations_type::iterator iter = outstanding_sig_registrs_.begin(); iter != outstanding_sig_registrs_.end(); ++iter)
      {
         if (iter->second == &s)
         {
            alreadyAttached = true;
            break;
         }
      }
      
      if (!alreadyAttached)
         outstanding_sig_registrs_[sequence_nr] = &s;
      
      return !alreadyAttached;
   }
   
   /// @return 0 if signal is not registered, the id otherwise
   uint32_t removeSignalRegistration(ClientSignalBase& s)
   {
      uint32_t rc = 0;
      
      for (sighandlers_type::iterator iter = sighandlers_.begin(); iter != sighandlers_.end(); ++iter)
      {
         if (iter->second == &s)
         {
            rc = iter->first;
            sighandlers_.erase(iter);
            break;
         }
      }
      
      // maybe still outstanding?
      for (outstanding_signalregistrations_type::iterator iter = outstanding_sig_registrs_.begin(); iter != outstanding_sig_registrs_.end(); ++iter)
      {
         if (iter->second == &s)
         {
            outstanding_sig_registrs_.erase(iter);
            break;
         }
      }
      
      return rc;
   }
   
   uint32_t generateSequenceNr() 
   {
      ++sequence_;
      return sequence_ == INVALID_SEQUENCE_NR ? ++sequence_ : sequence_;
   }
   
   void addClient(StubBase& clnt);
   
   /// no arguments version
   bool waitForResponse(const ClientResponseHolder& resp)
   {
      assert(resp.r_);
      assert(!running_);
      
      char* data = nullptr;
      size_t len = 0;
      
      int rc = loopUntil(resp.sequence_nr_, &data, &len);
      std::unique_ptr<char> raii(data);
      
      if (rc == 0)
      {
         ClientResponse<>* r = safe_cast<ClientResponse<>*>(resp.r_);
         assert(r);
      }
      
      return rc == 0;
   }
   
   /// at least one argument version
   template<typename T1, typename... T>
   bool waitForResponse(const ClientResponseHolder& resp, T1& t1, T&... t)
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
         
         Deserializer d(data, len);
         std::tuple<T1, T...> tup;
         d >> tup;
         
         assign<0>(tup, t1, t...);
      }
      
      return rc == 0;
   }

   int loopUntil(uint32_t sequence_nr = INVALID_SEQUENCE_NR, char** argData = nullptr, size_t* argLen = 0, unsigned int timeoutMs = 2000)
   {
      int retval = 0;
      running_ = true;
      
      do
      {
         retval = once_(sequence_nr, argData, argLen, timeoutMs);
      }
      while(running_);
      
      return retval;
   }
   
   inline
   int once(unsigned int timeoutMs = 2000)
   {
      char* data = nullptr;
      return once_(INVALID_SEQUENCE_NR, &data, 0, timeoutMs);
   }
   
private:
   
   
   void serviceReady(StubBase* stub, const std::string& fullName, const std::string& location)
   {
      assert(fullQualifiedName(*stub) == fullName);
      connect(*stub, false, location.c_str());
   }
   
   
   int accept_socket(int acceptor)
   {
      // doesn't matter which type of socket here
      union
      {
         struct sockaddr_un u_addr;
         struct sockaddr_in i_addr;
      } u;
      
      socklen_t len = sizeof(u);

      int fd = ::accept(acceptor, (struct sockaddr*)&u, &len);
      if (fd > 0)
      {
         fds_[fd].fd = fd;
         fds_[fd].events = POLLIN;
      }
      
      return fd;
   }
   
   
   int once_(uint32_t sequence_nr, char** argData, size_t* argLen, unsigned int timeoutMs)
   {
      int retval = 0;
      
      int rc = ::poll(fds_, sizeof(fds_)/sizeof(fds_[0]), timeoutMs);
            
      if (rc > 0)
      {
         for(unsigned int i=1; i<sizeof(fds_)/sizeof(fds_[0]); ++i)
         {
            if (fds_[i].revents & POLLIN)
            {
               if (af_[i] != 0)
               {
                  // acceptor socket
                  int new_fd = (this->*af_[i])(fds_[i].fd);
                  
                  if (new_fd > 0)
                     socketConnected(new_fd);
               }
               else
               {
                  // ordinary stream socket
                  FrameHeader f;
                     
                  ssize_t len = ::recv(fds_[i].fd, &f, sizeof(f), MSG_NOSIGNAL|MSG_WAITALL|MSG_PEEK);
                  
                  if (len == sizeof(f) && f)
                  {
                     len = 0;
                     std::unique_ptr<char> buf;   // it's safe since POD array though scoped_array would be better here!
                     
                     union Hdr 
                     {
                        Hdr()
                        {
                           // NOOP
                        }
                        
                        RequestFrame rqf;
                        ResponseFrame rf;
                        InterfaceResolveFrame irf;
                        InterfaceResolveResponseFrame irrf;
                        RegisterSignalFrame rsf;
                        UnregisterSignalFrame usf;
                        SignalEmitFrame sef;
                        SignalResponseFrame srf;
                        TransportErrorFrame tef;
                     } hdr; 

                     struct msghdr msg;
                     memset(&msg, 0, sizeof(msg));
                     
                     struct iovec v[2] = { { &hdr, headersize[f.type_] }, { 0, 0 } };
                     msg.msg_iov = v;
                     
                     if (f.payloadsize_ > 0)
                     {
                        msg.msg_iovlen = 2;
                        
                        buf.reset(new char[f.payloadsize_]);        
                        
                        v[1].iov_base = buf.get();
                        v[1].iov_len = f.payloadsize_;
                     }
                     else
                        msg.msg_iovlen = 1;
                                                
                     len = ::recvmsg(fds_[i].fd, &msg, MSG_NOSIGNAL|MSG_WAITALL);

                     // all data read?
                     if (len == v[0].iov_len + v[1].iov_len)
                     {
                        switch(f.type_)
                        {
                        case FRAME_TYPE_REQUEST:
                           { 
                              servermapid_type::iterator iter = servers_by_id_.find(hdr.rqf.serverid_);
                              if (iter != servers_by_id_.end())
                              {
                                 iter->second->eval(hdr.rqf.func_, hdr.rqf.sequence_nr_, hdr.rqf.sessionid_, fds_[i].fd, buf.get(), hdr.rqf.payloadsize_);
                              }
                              else
                                 std::cerr << "No service with id=" << hdr.rqf.serverid_ << " found." << std::endl;
                           }
                           break;
                           
                        case FRAME_TYPE_TRANSPORT_ERROR:
                           if (sequence_nr == INVALID_SEQUENCE_NR || sequence_nr != hdr.tef.sequence_nr_)
                           {
                              CallState cs((TransportError*)(hdr.tef.err_));
                              ((ClientResponseBase*)hdr.tef.handler_)->eval(cs, 0, 0);
                           }
                           else
                           {
                              std::unique_ptr<TransportError> var((TransportError*)(hdr.tef.err_));
                              throw *var;
                           }
                        
                           break;
                           
                        case FRAME_TYPE_RESPONSE:
                           {  
                              outstanding_requests_type::iterator iter;
                              if ((iter = outstandings_.find(hdr.rf.sequence_nr_)) != outstandings_.end())
                              {
                                 if (sequence_nr == INVALID_SEQUENCE_NR || hdr.rf.sequence_nr_ != sequence_nr)
                                 {
                                    if (hdr.rf.result_ == 0)   // normal response
                                    {
                                       CallState cs(hdr.rf.sequence_nr_);
                                       std::get<1>(iter->second)->eval(cs, buf.get(), hdr.rf.payloadsize_);
                                    }
                                    else   // error response
                                    {
                                       CallState cs(new RuntimeError(hdr.rf.result_, (const char*)buf.get(), hdr.rf.sequence_nr_));
                                       std::get<1>(iter->second)->eval(cs, 0, 0);
                                    }
                                 }
                                 else
                                 {
                                    assert(argLen);
                                    running_ = false;
                                    
                                    if (hdr.rf.result_ == 0)
                                    {
                                       if (buf.get() == 0)
                                       {
                                          // must copy payload in this case
                                          *argData = new char[hdr.rf.payloadsize_];
                                          ::memcpy(*argData, buf.get(), hdr.rf.payloadsize_);
                                       }
                                       else
                                          *argData = buf.release();
                                       
                                       *argLen = hdr.rf.payloadsize_;
                                    }
                                    else
                                       throw RuntimeError(hdr.rf.result_, (const char*)buf.get(), hdr.rf.sequence_nr_);
                                 }
                                 
                                 outstandings_.erase(iter);
                              }
                              else
                                 std::cerr << "Got response for request never sent..." << std::endl;
                           }
                           break;
                        
                        case FRAME_TYPE_REGISTER_SIGNAL:
                           {
                              servermapid_type::iterator iter = servers_by_id_.find(hdr.rsf.serverid_);
                              if (iter != servers_by_id_.end())
                              {
                                 uint32_t registrationid = generateId();
                                 
                                 ServerSignalBase* sig = iter->second->addSignalRecipient(hdr.rsf.sig_, fds_[i].fd, registrationid, hdr.rsf.id_);
                                 
                                 if (sig)
                                    server_sighandlers_[registrationid] = sig;
                                 
                                 // FIXME error handling, what if sig is 0???
                                 SignalResponseFrame rf(registrationid, hdr.rsf.id_);
                                 rf.sequence_nr_ = hdr.rsf.sequence_nr_;
                                 
                                 genericSend(fds_[i].fd, rf, 0);
                                 
                                 if (sig)
                                    sig->onAttach(registrationid);
                              }
                              else
                                 std::cerr << "No server with id=" << hdr.rsf.serverid_ << " found." << std::endl;
                           }
                           break;
                           
                        case FRAME_TYPE_UNREGISTER_SIGNAL:
                           {
                              serversignalers_type::iterator iter = server_sighandlers_.find(hdr.usf.registrationid_);
                              if (iter != server_sighandlers_.end())
                              {
                                 iter->second->removeRecipient(hdr.usf.registrationid_);
                              }
                              else
                                 std::cerr << "No registered signal '" << hdr.usf.registrationid_ << "' found." << std::endl;
                           }
                           break;
                        
                        case FRAME_TYPE_REGISTER_SIGNAL_RESPONSE:
                           {
                              outstanding_signalregistrations_type::iterator iter = outstanding_sig_registrs_.find(hdr.srf.sequence_nr_);
                              if (iter != outstanding_sig_registrs_.end())
                              {
                                 sighandlers_[hdr.srf.id_] = iter->second;
                                 outstanding_sig_registrs_.erase(iter);
                              }
                              else
                                 std::cerr << "No such signal registration found." << std::endl;
                              
                              if (sequence_nr == hdr.srf.sequence_nr_)
                                 running_ = false;
                           }
                           break;

                        case FRAME_TYPE_SIGNAL:
                           {
                              sighandlers_type::iterator iter = sighandlers_.find(hdr.sef.id_);
                              if (iter != sighandlers_.end())
                              {
                                 iter->second->eval(buf.get(), hdr.sef.payloadsize_);
                              }
                              else
                                 std::cerr << "No such signal handler found." << std::endl;
                           }
                           break;
                      
                        case FRAME_TYPE_RESOLVE_INTERFACE:
                           {
                              InterfaceResolveResponseFrame rf(0, generateId());
                              rf.sequence_nr_ = hdr.irf.sequence_nr_;
                                 
                              servermap_type::iterator iter = servers_.find(std::string((char*)buf.get()));
                              if (iter != servers_.end())
                              {
                                 for(servermapid_type::iterator iditer = servers_by_id_.begin(); iditer != servers_by_id_.end(); ++iditer)
                                 {
                                    if (iter->second == iditer->second)
                                    {
                                       rf.id_ = iditer->first;
                                       break;
                                    }
                                 }
                              }
                              else
                                 std::cerr << "No such server found." << std::endl;
                              
                              genericSend(fds_[i].fd, rf, 0);
                           }
                           break;
                        
                        case FRAME_TYPE_RESOLVE_RESPONSE_INTERFACE:
                           {
                              outstanding_interface_resolves_type::iterator iter = dangling_interface_resolves_.find(hdr.irrf.sequence_nr_);
                              if (iter != dangling_interface_resolves_.end())
                              {
                                 StubBase* stub = iter->second;
                                 stub->id_ = hdr.irrf.id_;
                                 stub->current_sessionid_ = hdr.irrf.sessionid_;
                                 dangling_interface_resolves_.erase(iter);
                                 
                                 if (sequence_nr == INVALID_SEQUENCE_NR)
                                 {
                                    // eventloop driven
                                    if (!iter->second->connected)
                                    {
                                        std::cerr << "'connected' hook not implemented. Client will probably hang..." << std::endl;
                                    }
                                    else
                                        iter->second->connected();
                                 }
                                 else if (sequence_nr == hdr.irrf.sequence_nr_)
                                    running_ = false;
                              }
                           }
                           break;
                        
                        default:
                           std::cerr << "Unimplemented frame type=" << f.type_ << std::endl;
                           break;
                        }
                     }
                     else
                        clearSlot(i);
                  }
                  else
                     clearSlot(i);                      
                  
                  break;
               }
            }                  
         }
      }
      else if (rc < 0)
      {
         if (errno != EINTR)
         {
            retval = -1;
            running_ = false;
         }
      }
      
      return retval;
   }
   
public:
   
   int run()
   {
      running_ = true;
      
      for(clientmap_type::iterator iter = clients_.begin(); iter != clients_.end(); ++iter)
      {
         if (strcmp(iter->second->boundname_, "auto:"))
         {
            if (iter->second->fd_ <= 0)
               assert(connect(*iter->second));
         }
      }
      
      // now enter infinite eventloop
      loopUntil();
   }
   
   void clearSlot(int idx)
   {
      while(::close(fds_[idx].fd) && errno == EINTR);
      
      for (serversignalers_type::iterator iter = server_sighandlers_.begin(); iter != server_sighandlers_.end(); ++iter)
      {
         iter->second->removeAllWithFd(fds_[idx].fd);
      }
      
      for (sessionmap_type::iterator iter = sessions_.begin(); iter != sessions_.end();)
      {
         if (iter->second.fd_ == fds_[idx].fd)
         {
            (*iter->second.destructor_)(iter->second.data_);
            sessions_.erase(iter);
            iter = sessions_.begin();
         }
         else
            ++iter;
      }
      
      socketDisconnected(fds_[idx].fd);
                        
      fds_[idx].fd = 0;
      fds_[idx].events = 0;
   }
   
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
   
   inline
   void* getSessionData(uint32_t sessionid)
   {
      sessionmap_type::iterator iter = sessions_.find(sessionid);
      if (iter != sessions_.end())
         return iter->second.data_;
      
      return 0;
   }
   
   inline
   void registerSession(uint32_t fd, uint32_t sessionid, void* data, void(*destructor)(void*))
   {
      sessions_[sessionid] = SessionData(fd, data, destructor);
   }
   
   
private:
   
   bool connect(StubBase& stub, bool blockUntilResponse = false, const char* location = 0)
   {
      const char* the_location = location ? location : stub.boundname_;
      
      assert(strcmp(the_location, "auto:"));
      
      // 1. connect the socket physically - if not yet done
      socketsmap_type::iterator iter = socks_.find(the_location);
      if (iter != socks_.end())
      {
         stub.fd_ = iter->second;
      }
      else
      {
         int rc = -1;
         
         if (!strncmp(the_location, "unix:", 5))
         {
            stub.fd_ = ::socket(PF_UNIX, SOCK_STREAM, 0);
      
            struct sockaddr_un addr;
            addr.sun_family = AF_UNIX;
            sprintf(addr.sun_path, "/tmp/dispatcher/%s", the_location + 5);
            
            rc = ::connect(stub.fd_, (struct sockaddr*)&addr, sizeof(addr));
         }
         else if (!strncmp(the_location, "tcp:", 4))
         {
            stub.fd_ = ::socket(PF_INET, SOCK_STREAM, 0);
      
            struct sockaddr_in addr;
            makeInetAddress(addr, the_location);
            
            rc = ::connect(stub.fd_, (struct sockaddr*)&addr, sizeof(addr));
            
            if (rc == 0)
            {
               int on = 1;
               rc = ::setsockopt(stub.fd_, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
            }
         }
         else
            return false;
      
         if (rc == 0)
         {
            fds_[stub.fd_].fd = stub.fd_;
            fds_[stub.fd_].events = POLLIN;
            
            socks_[the_location] = stub.fd_;
         }
         else
         {
            while(::close(stub.fd_) < 0 && errno == EINTR);
            stub.fd_ = -1;
         }
      }
      
      // 2. initialize interface resolution
      if (stub.fd_ > 0)
      {
         // don't cache anything here, a sessionid will be 
         // created no the server for any resolve request
         InterfaceResolveFrame f(42);
         char buf[128];
         
         assert(strlen(stub.iface_) + 2 + strlen(stub.role_) < sizeof(buf));
         fullQualifiedName(buf, stub.iface_, stub.role_);
                                 
         f.payloadsize_ = strlen(buf)+1;
         f.sequence_nr_ = generateSequenceNr();
   
         dangling_interface_resolves_[f.sequence_nr_] = &stub;
         genericSend(stub.fd_, f, buf);
         
         if (blockUntilResponse)
            loopUntil(f.sequence_nr_);
      }
         
      return stub.fd_ > 0;
   }

   
   //registered servers
   servermap_type servers_;
   servermapid_type servers_by_id_;
   outstanding_interface_resolves_type dangling_interface_resolves_;
   
   uint32_t nextid_;
   bool running_;
   
   pollfd fds_[32];
   
   typedef int(Dispatcher::*acceptfunction_type)(int);
   acceptfunction_type af_[32];   ///< may have different kind of sockets here (tcp or unix)
   
   // registered clients
   clientmap_type clients_;
   
   uint32_t sequence_;
   
   outstanding_requests_type outstandings_;
   outstanding_signalregistrations_type outstanding_sig_registrs_;
   sighandlers_type sighandlers_;   
   serversignalers_type server_sighandlers_;
   
   socketsmap_type socks_;
   
   BrokerClient* broker_;
   std::vector<std::string> endpoints_;
   
   sessionmap_type sessions_;
   
   int selfpipe_[2];
};


uint32_t StubBase::sendRequest(Parented& requestor, ClientResponseBase* handler, uint32_t id, const Serializer& s)
{
   assert(disp_);
 
   RequestFrame f(id_, id, current_sessionid_);
   f.payloadsize_ = s.size();
   f.sequence_nr_ = disp_->generateSequenceNr();
   
   if (genericSend(fd(), f, s.data()))
   {
      if (handler)
         disp_->addRequest(requestor, *handler, f.sequence_nr_);
   }
   else
   {
      // no fire-and-forget?
      if (handler)
      {
         errno = EINVAL;
         TransportError* err = new TransportError(errno, f.sequence_nr_);
         
         TransportErrorFrame ef(handler, err);
         ef.sequence_nr_ = f.sequence_nr_;
         
         (void)::write(disp_->selfpipe_[1], &ef, sizeof(ef));
      }
   }
   
   return f.sequence_nr_;
}


void StubBase::sendSignalRegistration(ClientSignalBase& sigbase)
{
   assert(disp_);
   
   RegisterSignalFrame f(id_, sigbase.id(), disp_->generateId());
   f.payloadsize_ = 0;
   f.sequence_nr_ = disp_->generateSequenceNr();
   
   if (disp_->addSignalRegistration(sigbase, f.sequence_nr_))
   {
      if (genericSend(fd(), f, 0))
      {
         if (!disp_->isRunning())
            disp_->loopUntil(f.sequence_nr_);
      }
      //else FIXME remove signal registration again
   }
}


inline
bool StubBase::connect(bool block)
{
   return disp_->connect(*this, block);
}


void StubBase::sendSignalUnregistration(ClientSignalBase& sigbase)
{
   assert(disp_);
   
   UnregisterSignalFrame f(disp_->removeSignalRegistration(sigbase));
   
   if (f.registrationid_ != 0)
   {
      f.payloadsize_ = 0;
      f.sequence_nr_ = disp_->generateSequenceNr();
      
      genericSend(fd(), f, 0);
   }
}


template<template<template<typename...> class, 
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,typename> class> 
   class IfaceT>
struct Stub : StubBase, IfaceT<ClientRequest, ClientResponse, ClientSignal, ClientAttribute>
{   
   friend struct Dispatcher;
   
private:

   typedef IfaceT<ClientRequest, ClientResponse, ClientSignal, ClientAttribute> interface_type;
   
public:
   
   Stub(const char* role, const char* boundname)
    : StubBase(InterfaceNamer<interface_type>::name(), role, boundname)
   {
      std::for_each(((interface_type*)this)->container_.begin(), ((interface_type*)this)->container_.end(), std::bind(&StubBase::reparent, this, _1));
      ((interface_type*)this)->container_.clear();
   }
   
   /// blocking connect to server
   inline
   bool connect()
   {
      assert(!disp_->isRunning());
      
      bool rc = true;
      
      if (fd_ <= 0)
         rc = StubBase::connect(true);   // inherited friendship
      
      return rc;
   }
};


/// this class supports only move semantics!
struct ServerRequestDescriptor
{
   inline
   ServerRequestDescriptor()
    : requestor_(0)
    , fd_(-1)
    , sequence_nr_(INVALID_SEQUENCE_NR)
    , sessionid_(INVALID_SESSION_ID)
   {
      // NOOP
   }
   
   inline
   ServerRequestDescriptor(const ServerRequestDescriptor& rhs)
    : requestor_(rhs.requestor_)
    , fd_(rhs.fd_)
    , sequence_nr_(rhs.sequence_nr_)
    , sessionid_(rhs.sessionid_)
   {
      const_cast<ServerRequestDescriptor&>(rhs).clear();
   }
   
   ServerRequestDescriptor& operator=(const ServerRequestDescriptor& rhs)
   {
      if (this != &rhs)
      {
         requestor_ = rhs.requestor_;
         fd_ = rhs.fd_;
         sequence_nr_ = rhs.sequence_nr_;
         sessionid_ = rhs.sessionid_;
         
         const_cast<ServerRequestDescriptor&>(rhs).clear();
      }
      
      return *this;
   }
   
   ServerRequestDescriptor& set(ServerRequestBase* requestor, int fd, uint32_t sequence_nr, uint32_t sessionid)
   {
      requestor_ = requestor;
      fd_ = fd;
      sequence_nr_ = sequence_nr;
      sessionid_ = sessionid;
      return *this;
   }
   
   inline
   void clear()
   {
      set(0, -1, INVALID_SEQUENCE_NR, INVALID_SESSION_ID);
   }
   
   inline
   operator const void*() const
   {
      return requestor_;
   }
   
   ServerRequestBase* requestor_;
   int fd_;
   uint32_t sequence_nr_;
   uint32_t sessionid_;
};


template<template<template<typename...> class, 
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,typename> class> class IfaceT>
struct Skeleton : IfaceT<ServerRequest, ServerResponse, ServerSignal, ServerAttribute>
{
   friend struct Dispatcher;
   template<typename SkelT> friend struct ServerHolder;
   
   typedef IfaceT<ServerRequest, ServerResponse, ServerSignal, ServerAttribute> interface_type;
   
   Skeleton(const char* role)
    : role_(role)
    , disp_(0)
   {
      assert(role_);
   }
   
   virtual ~Skeleton()
   {
      // NOOP
   }
   
   Dispatcher& disp()
   {
      assert(disp_);
      return *disp_;
   }
   
   /// only valid within request handler - must be called in order to respond to the request later in time
   inline
   ServerRequestDescriptor deferResponse()
   {
      assert(current_request_);
      assert(current_request_.requestor_->hasResponse());  
      
      return current_request_;   // invalidates the current request!
   }
   
   /// only valid to call within request handler
   void respondWith(ServerResponseHolder response)
   {
      assert(current_request_);
      assert(response.responder_->allowedRequests_.find(current_request_.requestor_) != response.responder_->allowedRequests_.end());
      
      ResponseFrame r(0);
      r.payloadsize_ = response.size_;
      r.sequence_nr_ = current_request_.sequence_nr_;
      
      genericSend(current_request_.fd_, r, response.payload_);
      current_request_.clear();   // only respond once!!!
   }
   
   /// send deferred response as retrieved by calling deferResponse()
   void respondOn(ServerRequestDescriptor& req, ServerResponseHolder response)
   {
      assert(req);
      assert(response.responder_->allowedRequests_.find(req.requestor_) != response.responder_->allowedRequests_.end());
      
      ResponseFrame r(0);
      r.payloadsize_ = response.size_;
      r.sequence_nr_ = req.sequence_nr_;
      
      genericSend(req.fd_, r, response.payload_);
      req.clear();
   }
   
   /// send error response - only valid to call within request handler
   void respondWith(const RuntimeError& err)
   {
      assert(current_request_);
      assert(current_request_.requestor_->hasResponse());
      
      ResponseFrame r(err.error());
      r.payloadsize_ = err.what() ? strlen(err.what()) + 1 : 0;
      r.sequence_nr_ = current_request_.sequence_nr_;
      
      genericSend(current_request_.fd_, r, err.what());
      current_request_.clear();   // only respond once!!!
   }
   
   /// send deferred error response as retrieved by calling deferResponse()
   void respondOn(ServerRequestDescriptor& req, const RuntimeError& err)
   {
      assert(req);
      assert(req.requestor_->hasResponse());
      
      ResponseFrame r(err.error());
      r.payloadsize_ = err.what() ? strlen(err.what()) + 1 : 0;
      r.sequence_nr_ = req.sequence_nr_;
      
      genericSend(req.fd_, r, err.what());
      req.clear();
   }
   
   inline
   const ServerRequestDescriptor& currentRequest() const
   {
      assert(current_request_);
      return current_request_;
   }
   
   inline
   void* currentSessionData()
   {
      return disp().getSessionData(currentRequest().sessionid_);
   }
   
   inline
   void registerSession(void* data, void(*destructor)(void*))
   {
      disp().registerSession(currentRequest().fd_, currentRequest().sessionid_, data, destructor);
   }
   
   
private:
   
   void handleRequest(uint32_t funcid, uint32_t sequence_nr, uint32_t sessionid, int fd, const void* payload, size_t length)
   {
      //std::cout << "Skeleton::handleRequest '" << funcid << "'" << std::endl;
      std::map<uint32_t, ServerRequestBase*>::iterator iter = ((interface_type*)this)->container_.find(funcid);
      
      if (iter != ((interface_type*)this)->container_.end())
      {
         try
         {
            current_request_.set(iter->second, fd, sequence_nr, sessionid);
            iter->second->eval(payload, length);
            
            // current_request_ is only valid if no response handler was called
            if (current_request_)
            {
                // in that case the request must not have a reponse
               assert(!current_request_.requestor_->hasResponse());  
               current_request_.clear();
            }
         }
         catch(const RuntimeError& err)
         {
            if (current_request_.requestor_->hasResponse())
            {
               respondWith(err);
               current_request_.clear();
            }
         }
         catch(...)
         {
            if (current_request_.requestor_->hasResponse())
            {
               RuntimeError err(-1, "Unknown unhandled exception occured on server");
               respondWith(err);
            }
               
            current_request_.clear();
            throw;
         }
      }
      else
         std::cerr << "Unknown request '" << funcid << "' with payload size=" << length << std::endl;
   }
   
   /// return a session pointer and destruction function if adequate
   virtual std::tuple<void*,void(*)(void*)> clientAttached()
   {
      abort();
      std::cout << "XX" << std::endl;
      return std::tuple<void*,void(*)(void*)>(nullptr, nullptr);   // the default does not create any session data
   }
   
   const char* role_;
   Dispatcher* disp_;
   ServerRequestDescriptor current_request_;
};


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


// -------------------------------------------------------------------------------------------------


/// for storing a function in the connected function object
template<typename CallableT>
inline
void operator>> (std::function<void()>& func, const CallableT& callable)
{
   func = callable;
}


// -------------------------------------------------------------------------------------------------


template<typename... T, typename... T2>
inline
void operator>> (ClientRequest<T...>& request, ClientResponse<T2...>& response)
{
   assert(!request.handler_);
   request.handler_ = &response;
}


template<typename... T, typename... T2>
inline
void operator>> (ServerRequest<T...>& request, ServerResponse<T2...>& response)
{
   assert(!request.hasResponse());
   
   ServerRequestBaseSetter::setHasResponse(request);
   response.allowedRequests_.insert(&request);   
}


// -------------------------------------------------------------------------------------------------


struct AbsoluteInterfaceBase
{      
   inline
   uint32_t nextId()
   {
      return ++id_;
   }
   
protected:

   inline
   AbsoluteInterfaceBase()
    : id_(0)
   {
      // NOOP
   }

   inline
   ~AbsoluteInterfaceBase()
   {
      // NOOP
   }
   
   uint32_t id_;   // mutable variable for request/signals id registration, after startup this var has an senseless value
};


template<template<typename...> class RequestT>
struct InterfaceBase;


template<>
struct InterfaceBase<ClientRequest> : AbsoluteInterfaceBase
{
   inline
   InterfaceBase()
    : signals_(container_)
   {
      // NOOP
   }
   
   // temporary lists
   std::vector<Parented*> container_;
   std::vector<Parented*>& signals_;
};


template<>
struct InterfaceBase<ServerRequest> : AbsoluteInterfaceBase
{
   std::map<uint32_t, ServerRequestBase*> container_;
   std::map<uint32_t, ServerSignalBase*> signals_;
};


#define INTERFACE(iface) \
   template<template<typename...> class Request, \
            template<typename...> class Response, \
            template<typename...> class Signal, \
            template<typename,typename=OnChange> class Attribute> \
      struct iface; \
            \
   template<> struct InterfaceNamer<iface<ClientRequest, ClientResponse, ClientSignal, ClientAttribute> > { static inline const char* const name() { return #  iface ; } }; \
   template<> struct InterfaceNamer<iface<ServerRequest, ServerResponse, ServerSignal, ServerAttribute> > { static inline const char* const name() { return #  iface ; } }; \
   template<template<typename...> class Request, \
            template<typename...> class Response, \
            template<typename...> class Signal, \
            template<typename,typename=OnChange> class Attribute> \
      struct iface : InterfaceBase<Request>

#define INIT_REQUEST(request) \
   request(((AbsoluteInterfaceBase*)this)->nextId(), ((InterfaceBase<Request>*)this)->container_)

#define INIT_RESPONSE(response) \
   response()

#define INIT_SIGNAL(signal) \
   signal(((AbsoluteInterfaceBase*)this)->nextId(), ((InterfaceBase<Request>*)this)->signals_)

// an attribute is nothing more that an encapsulated signal
#define INIT_ATTRIBUTE(attr) \
   attr(((AbsoluteInterfaceBase*)this)->nextId(), ((InterfaceBase<Request>*)this)->signals_)

   
#include "brokerclient.h"


Dispatcher::~Dispatcher()
{
   for(servermap_type::iterator iter = servers_.begin(); iter != servers_.end(); ++iter)
   {
      delete iter->second;
   }
   
   if (broker_)
   {
      delete broker_;
      broker_ = 0;
   }
   
   while(::close(selfpipe_[0]) && errno == EINTR);
   while(::close(selfpipe_[1]) && errno == EINTR);
}


void Dispatcher::addClient(StubBase& clnt)
{
   assert(!clnt.disp_);   // don't add it twice
   
   clnt.disp_ = this;
   clients_.insert(std::make_pair(fullQualifiedName(clnt), &clnt)); 
   
   if (!strcmp(clnt.boundname_, "auto:"))
   {
      assert(broker_);
      broker_->waitForService(fullQualifiedName(clnt), std::bind(&Dispatcher::serviceReady, this, &clnt, _1, _2));
   }
   else
   {
      if (isRunning())
         connect(clnt);
   }
}


template<typename ServerT>
void Dispatcher::addServer(ServerT& serv)
{
   static_assert(isServer<ServerT>::value, "only_add_servers_here");
   assert(!endpoints_.empty());
   
   std::string name = fullQualifiedName(InterfaceNamer<typename ServerT::interface_type>::name(), serv.role_);
   
   assert(servers_.find(name) == servers_.end());
   std::cout << "Adding server for '" << name << "'" << std::endl;
   
   if (broker_)
   {
      // FIXME must register service for multiple endpoints
      // FIXME must resolve INADDR_ANY to senseful address list for registration
      broker_->registerService(name, endpoints_.front());
   }
   
   ServerHolder<ServerT>* holder = new ServerHolder<ServerT>(serv);
   servers_[name] = holder;
   servers_by_id_[generateId()] = holder;
   
   serv.disp_ = this;
}

inline
void Dispatcher::enableBrokerage()
{
   broker_ = new BrokerClient(*this);
}


#endif   // IPC2_H
