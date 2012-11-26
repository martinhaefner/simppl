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
#include "variant.h"   // for retrieving header sizes
#include "noninstantiable.h"

#include "detail/serialization.h"

#define INVALID_SEQUENCE_NR 0xFFFFFFFF
#define INVALID_SERVER_ID 0xFFFFFFFFu
#define RESERVED_SERVER_ID 0u   // no distinct server, just the dispatcher has to resolve the request
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

// no forward decl, just a type definition
struct Void;

template<typename T>
struct isVoid
{
   enum { value = std::is_same<T, Void>::value };
};


#ifdef SIMPPL_HAVE_VALIDATION
#   include "detail/validation.h"
#else

template<typename T>
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
   
   // FIXME add access to exception itself
   
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


template<typename T1, typename T2, typename T3>
struct DeserializeAndCall : NonInstantiable
{
   template<typename FunctorT>
   static inline
   void eval(Deserializer& d, FunctorT& f)
   {
      T1 t1;
      T2 t2;
      T3 t3;
      
      d >> t1 >> t2 >> t3;
      f(t1, t2, t3);
   }
   
   
   template<typename FunctorT>
   static inline
   void evalResponse(Deserializer& d, FunctorT& f, const CallState& cs)
   {
      T1 t1;
      T2 t2;
      T3 t3;
      
      if (cs)
         d >> t1 >> t2 >> t3;
      
      f(cs, t1, t2, t3);
   }
};


template<>
struct DeserializeAndCall<Void, Void, Void> : NonInstantiable
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

template<typename T>
struct DeserializeAndCall<T, Void, Void> : NonInstantiable
{
   template<typename FunctorT>
   static inline
   void eval(Deserializer& d, FunctorT& f)
   {
      T t;
      
      d >> t;
      f(t);
   }
   
   template<typename FunctorT>
   static inline
   void evalResponse(Deserializer& d, FunctorT& f, const CallState& cs)
   {
      T t;
      
      if (cs)
         d >> t;
      
      f(cs, t);
   }
};

template<typename T1, typename T2>
struct DeserializeAndCall<T1, T2, Void> : NonInstantiable
{
   template<typename FunctorT>
   static inline
   void eval(Deserializer& d, FunctorT& f)
   {
      T1 t1;
      T2 t2;
      
      d >> t1 >> t2;
      f(t1, t2);
   }
   
   template<typename FunctorT>
   static inline
   void evalResponse(Deserializer& d, FunctorT& f, const CallState& cs)
   {
      T1 t1;
      T2 t2;
      
      if (cs)
         d >> t1 >> t2;
      
      f(cs, t1, t2);
   }
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

#define REQUEST_RESOLVE_INTERFACE 0x1u

#define FRAME_MAGIC                            0xAABBCCDDu
#define FRAME_TYPE_REQUEST                     0x1u
#define FRAME_TYPE_RESPONSE                    0x2u
#define FRAME_TYPE_RESOLVE_RESPONSE_INTERFACE  0x3u
#define FRAME_TYPE_REGISTER_SIGNAL             0x4u
#define FRAME_TYPE_UNREGISTER_SIGNAL           0x5u
#define FRAME_TYPE_SIGNAL                      0x6u
#define FRAME_TYPE_REGISTER_SIGNAL_RESPONSE    0x7u
#define FRAME_TYPE_TRANSPORT_ERROR             0x8u


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

// ------------------------------------------------------------------------------------------


struct HeaderSize
{
   typedef TYPELIST_8( \
      InterfaceResolveResponseFrame, \
      RequestFrame, \
      ResponseFrame, \
      RegisterSignalFrame, \
      UnregisterSignalFrame, \
      SignalEmitFrame, \
      SignalResponseFrame, \
      TransportErrorFrame \
   ) 
   headertypes;

   enum { max = Max<headertypes, SizeFunc>::value };
   
   static const unsigned int size[9];
};


/*static*/ 
const unsigned int HeaderSize::size[9] = {
   0
 , sizeof(RequestFrame)
 , sizeof(ResponseFrame)
 , sizeof(InterfaceResolveResponseFrame) 
 , sizeof(RegisterSignalFrame)
 , sizeof(UnregisterSignalFrame)
 , sizeof(SignalEmitFrame)
 , sizeof(SignalResponseFrame)
 , sizeof(TransportErrorFrame)
};

static_assert((sizeof(HeaderSize::size) / sizeof(HeaderSize::size[0])) == Size<HeaderSize::headertypes>::value + 1, "error_frame_types_must_occur_here");


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
   template<typename T1, typename T2, typename T3> friend struct ClientSignal;
   template<typename T1, typename T2, typename T3> friend struct ClientRequest;
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


template<typename T1, typename T2, typename T3>
struct ClientSignal : ClientSignalBase
{
   typedef typename CallTraits<T1>::param_type arg1_type;
   typedef typename CallTraits<T2>::param_type arg2_type;
   typedef typename CallTraits<T3>::param_type arg3_type;
   
   typedef 
      typename if_<isVoid<T3>::value, 
         typename if_<isVoid<T2>::value, 
            typename if_<isVoid<T1>::value, std::function<void()>, 
               /*else*/std::function<void(arg1_type)> >::type,    
            /*else*/std::function<void(arg1_type, arg2_type)> >::type, 
         /*else*/std::function<void(arg1_type, arg2_type, arg3_type)> >::type 
      function_type;
      
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
         DeserializeAndCall<T1, T2, T3>::eval(d, f_);
      }
      else
         std::cerr << "No appropriate handler registered for signal " << id_ << " with payload size=" << length << std::endl;
   }
   
   function_type f_;
};


template<typename T1, typename T2, typename T3, typename FuncT>
inline
void operator>>(ClientSignal<T1, T2, T3>& sig, const FuncT& func)
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
   typedef ClientSignal<typename if_<is_vector<DataT>::value, ClientVectorAttributeUpdate<DataT>, DataT>::type, Void, Void> signal_type;

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

   /// normal argument
   inline
   void setAndCall(arg_type arg)
   {
      data_ = arg;

      if (f_)
         f_(data_);
   }

   void valueChanged(typename signal_type::arg1_type arg)
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


template<typename T1, typename T2, typename T3>
struct ServerSignal : ServerSignalBase
{
   typedef typename CallTraits<T1>::param_type arg1_type;
   typedef typename CallTraits<T2>::param_type arg2_type;
   typedef typename CallTraits<T3>::param_type arg3_type;
   
   inline
   ServerSignal(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
   {
      _signals[id] = this;
   }
   
   inline
   void emit()
   {
      static_assert(isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
      
      static Serializer s(0);
      sendSignal(s);
   }
   
   inline
   void emit(arg1_type arg1)
   {
      static_assert(!isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
    
      Serializer s(sizeof(T1));
      sendSignal(s << arg1);
   }
   
   inline
   void emit(arg1_type arg1, arg2_type arg2)
   {
      static_assert(!isVoid<T1>::value && !isVoid<T2>::value && isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
    
      Serializer s(sizeof(T1) + sizeof(T2));
      sendSignal(s << arg1 << arg2);
   }
   
   inline
   void emit(arg1_type arg1, arg2_type arg2, arg3_type arg3)
   {
      static_assert(!isVoid<T1>::value && !isVoid<T2>::value && !isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
    
      Serializer s(sizeof(T1) + sizeof(T2) + sizeof(T3));
      sendSignal(s << arg1 << arg2 << arg3);
   }

protected:
   inline
   void emit(arg1_type arg1, uint32_t registrationid)
   {
      Serializer s(sizeof(T1));
      s << arg1;

      SignalSender(s.data(), s.size())(ServerSignalBase::recipients_[registrationid]);
   }
};


template<typename DataT>
struct BaseAttribute 
 : ServerSignal<typename if_<is_vector<DataT>::value, ServerVectorAttributeUpdate<DataT>, DataT>::type, Void, Void> 
{
   inline
   BaseAttribute(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
    : ServerSignal<typename if_<is_vector<DataT>::value, ServerVectorAttributeUpdate<DataT>, DataT>::type, Void, Void>(id, _signals)
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
      emit(this->t_, registrationid);
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


template<typename T1, typename T2, typename T3>
struct ClientRequest : Parented
{
   typedef typename CallTraits<T1>::param_type arg1_type;
   typedef typename CallTraits<T2>::param_type arg2_type;
   typedef typename CallTraits<T3>::param_type arg3_type;
   
   inline
   ClientRequest(uint32_t id, std::vector<Parented*>& parent)
    : id_(id)
    , handler_(0)
   {
      static_assert(isValidType<T1>::value && isValidType<T2>::value && isValidType<T3>::value, "invalid_type_in_interface");
      parent.push_back(this);
   }
   
   inline
   ClientResponseHolder operator()()
   {
      static_assert(isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
      
      static Serializer s(0);
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(*this, handler_, id_, s));
   }
   
   inline
   ClientResponseHolder operator()(arg1_type t1)
   {
      static_assert(!isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
      
      Serializer s(sizeof(typename remove_ref<T1>::type));
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(*this, handler_, id_, s << t1));
   }

   inline
   ClientResponseHolder operator()(arg1_type t1, arg2_type t2)
   {
      static_assert(!isVoid<T1>::value && !isVoid<T2>::value && isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
      
      Serializer s(sizeof(typename remove_ref<T1>::type) + sizeof(typename remove_ref<T2>::type));
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(*this, handler_, id_, s << t1 << t2));
   }
   
   inline
   ClientResponseHolder operator()(arg1_type t1, arg2_type t2, arg3_type t3)
   {
      static_assert(!isVoid<T1>::value && !isVoid<T2>::value && !isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
      
      Serializer s(sizeof(typename remove_ref<T1>::type) + sizeof(typename remove_ref<T2>::type) + sizeof(typename remove_ref<T3>::type));
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(*this, handler_, id_, s << t1 << t2 << t3));
   }
   
   ClientResponseBase* handler_;
   uint32_t id_;
};


// ----------------------------------------------------------------------------------------


template<typename T1, typename T2, typename T3>
struct ClientResponse : ClientResponseBase
{
   typedef typename CallTraits<T1>::param_type arg1_type;
   typedef typename CallTraits<T2>::param_type arg2_type;
   typedef typename CallTraits<T3>::param_type arg3_type;
   
   typedef 
      typename if_<isVoid<T3>::value, 
         typename if_<isVoid<T2>::value, 
            typename if_<isVoid<T1>::value, std::function<void(const CallState&)>, 
               /*else*/std::function<void(const CallState&, arg1_type)> >::type,    
            /*else*/std::function<void(const CallState&, arg1_type, arg2_type)> >::type, 
         /*else*/std::function<void(const CallState&, arg1_type, arg2_type, arg3_type)> >::type 
      function_type;
   
   inline
   ClientResponse()
   {
      static_assert(isValidType<T1>::value && isValidType<T2>::value && isValidType<T3>::value, "invalid_type_in_interface");
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
         DeserializeAndCall<T1, T2, T3>::evalResponse(d, f_, cs);
      }
      else
         std::cerr << "No appropriate handler registered for response with payload size=" << length << std::endl;
   }
   
   function_type f_;
};



template<typename T1, typename T2, typename T3, typename FunctorT>
inline 
ClientResponse<T1, T2, T3>& operator>> (ClientResponse<T1, T2, T3>& r, const FunctorT& f)
{
   r.handledBy(f);
   return r;
}


// -------------------------------------------------------------------------------


template<typename T1, typename T2, typename T3>
struct ServerRequest : ServerRequestBase
{
   typedef typename CallTraits<T1>::param_type arg1_type;
   typedef typename CallTraits<T2>::param_type arg2_type;
   typedef typename CallTraits<T3>::param_type arg3_type;
   
   typedef 
      typename if_<isVoid<T3>::value, 
         typename if_<isVoid<T2>::value, 
            typename if_<isVoid<T1>::value, std::function<void()>, 
               /*else*/std::function<void(arg1_type)> >::type,    
            /*else*/std::function<void(arg1_type, arg2_type)> >::type, 
         /*else*/std::function<void(arg1_type, arg2_type, arg3_type)> >::type 
      function_type;
   
   inline
   ServerRequest(uint32_t id, std::map<uint32_t, ServerRequestBase*>& requests)
   {
      static_assert(isValidType<T1>::value && isValidType<T2>::value && isValidType<T3>::value, "invalid_type_in_interface");
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
         DeserializeAndCall<T1, T2, T3>::eval(d, f_);
      }
      else
         std::cerr << "No appropriate handler registered for request with payload size=" << length << std::endl;
   }
   
   function_type f_;
};


template<typename T1, typename T2, typename T3, typename FunctorT>
inline 
void operator>> (ServerRequest<T1, T2, T3>& r, const FunctorT& f)
{
   r.handledBy(f);
}


// ---------------------------------------------------------------------------------------------


template<typename T1, typename T2, typename T3>
struct ServerResponse : ServerResponseBase
{
   typedef typename CallTraits<T1>::param_type arg1_type;
   typedef typename CallTraits<T2>::param_type arg2_type;
   typedef typename CallTraits<T3>::param_type arg3_type;
   
   inline
   ServerResponse()
   {
      static_assert(isValidType<T1>::value && isValidType<T2>::value && isValidType<T3>::value, "invalid_type_in_interface");
   }
   
   inline
   ServerResponseHolder operator()(arg1_type t1)
   {
      static_assert(!isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
      
      Serializer s(sizeof(T1));      
      return ServerResponseHolder(s << t1, *this);
   }
   
   inline
   ServerResponseHolder operator()(arg1_type t1, arg2_type t2)
   {
      static_assert(!isVoid<T1>::value && !isVoid<T2>::value && isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
      
      Serializer s(sizeof(T1) + sizeof(T2));      
      return ServerResponseHolder(s << t1 << t2, *this);
   }
   
   inline
   ServerResponseHolder operator()(arg1_type t1, arg2_type t2, arg3_type t3)
   {
      static_assert(!isVoid<T1>::value && !isVoid<T2>::value && !isVoid<T3>::value, "invalid_function_call_due_to_arguments_mismatch");
      
      Serializer s(sizeof(T1) + sizeof(T2) + sizeof(T3));      
      return ServerResponseHolder(s << t1 << t2 << t3, *this);
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
   
   template<typename T1>
   bool waitForResponse(const ClientResponseHolder& resp, T1& t1)
   {
      assert(resp.r_);
      assert(!running_);
      
      char* data = nullptr;
      size_t len = 0;
      
      int rc = loopUntil(resp.sequence_nr_, &data, &len);
      std::unique_ptr<char> raii(data);
      
      if (rc == 0)
      {
         ClientResponse<T1, Void, Void>* r = safe_cast<ClientResponse<T1, Void, Void>*>(resp.r_);
         assert(r);
         
         Deserializer d(data, len);
         d >> t1;
      }
      
      return rc == 0;
   }
   
   template<typename T1, typename T2>
   bool waitForResponse(const ClientResponseHolder& resp, T1& t1, T2& t2)
   {
      assert(resp.r_);
      assert(!running_);
      
      char* data = nullptr;
      size_t len = 0;
      
      int rc = loopUntil(resp.sequence_nr_, &data, &len);
      std::unique_ptr<char> raii(data);
      
      if (rc == 0)
      {
         ClientResponse<T1, T2, Void>* r = safe_cast<ClientResponse<T1, T2, Void>*>(resp.r_);
         assert(r);
         
         Deserializer d(data, len);
         d >> t1 >> t2;
      }
      
      return rc == 0;
   }
   
   template<typename T1, typename T2, typename T3>
   bool waitForResponse(const ClientResponseHolder& resp, T1& t1, T2& t2, T3& t3)
   {
      assert(resp.r_);
      assert(!running_);
      
      char* data = nullptr;
      size_t len = 0;
      
      int rc = loopUntil(resp.sequence_nr_, &data, &len);
      std::unique_ptr<char> raii(data);
      
      if (rc == 0)
      {
         ClientResponse<T1, T2, T3>* r = safe_cast<ClientResponse<T1, T2, T3>*>(resp.r_);
         assert(r);
         
         Deserializer d(data, len);
         d >> t1 >> t2 >> t3;
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
                     
                     char hdr[HeaderSize::max] __attribute__((aligned(4)));

                     struct msghdr msg;
                     memset(&msg, 0, sizeof(msg));
                     
                     struct iovec v[2] = { { hdr, HeaderSize::size[f.type_] }, { 0, 0 } };
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
                              RequestFrame* rf = (RequestFrame*)hdr;
                              
                              if (rf->serverid_ != RESERVED_SERVER_ID)
                              {
                                 servermapid_type::iterator iter = servers_by_id_.find(rf->serverid_);
                                 if (iter != servers_by_id_.end())
                                 {
                                    iter->second->eval(rf->func_, rf->sequence_nr_, rf->sessionid_, fds_[i].fd, buf.get(), rf->payloadsize_);
                                 }
                                 else
                                    std::cerr << "No service with id=" << rf->serverid_ << " found." << std::endl;
                              }
                              else
                              {
                                 switch(rf->func_)
                                 {
                                 case REQUEST_RESOLVE_INTERFACE:
                                    {
                                       Deserializer ds(buf.get());
                                       std::string iface_name;
                                       ds >> iface_name;
                                       
                                       uint32_t the_id = INVALID_SERVER_ID;
                                       
                                       servermap_type::iterator iter = servers_.find(iface_name);
                                       if (iter != servers_.end())
                                       {
                                          for(servermapid_type::iterator iditer = servers_by_id_.begin(); iditer != servers_by_id_.end(); ++iditer)
                                          {
                                             if (iter->second == iditer->second)
                                             {
                                                the_id = iditer->first;
                                                break;
                                             }
                                          }
                                       }
                                       else
                                          std::cerr << "No such server '" << iface_name << "' found." << std::endl;
                                       
                                       sendInternalResponse(fds_[i].fd, RESPONSE_RESOLVE_INTERFACE, the_id, rf->sequence_nr_);
                                    }
                                    break;
                                    
                                 default:
                                    std::cerr << "Unknown internal request." << std::endl;
                                    break;
                                 }
                              }
                           }
                           break;
                           
                        case FRAME_TYPE_TRANSPORT_ERROR:
                           {
                              TransportErrorFrame* ef = (TransportErrorFrame*)hdr;
                              
                              if (sequence_nr == INVALID_SEQUENCE_NR || sequence_nr != ef->sequence_nr_)
                              {
                                 CallState cs((TransportError*)(ef->err_));
                                 ((ClientResponseBase*)ef->handler_)->eval(cs, 0, 0);
                              }
                              else
                              {
                                 std::unique_ptr<TransportError> var((TransportError*)(ef->err_));
                                 throw *var;
                              }
                           }
                           break;
                           
                        case FRAME_TYPE_RESPONSE:
                           {
                              ResponseFrame* rf = (ResponseFrame*)hdr;
                              
                              outstanding_requests_type::iterator iter;
                              if ((iter = outstandings_.find(rf->sequence_nr_)) != outstandings_.end())
                              {
                                 if (sequence_nr == INVALID_SEQUENCE_NR || rf->sequence_nr_ != sequence_nr)
                                 {
                                    if (rf->result_ == 0)   // normal response
                                    {
                                       CallState cs(rf->sequence_nr_);
                                       std::get<1>(iter->second)->eval(cs, buf.get(), rf->payloadsize_);
                                    }
                                    else   // error response
                                    {
                                       CallState cs(new RuntimeError(rf->result_, (const char*)buf.get(), rf->sequence_nr_));
                                       std::get<1>(iter->second)->eval(cs, 0, 0);
                                    }
                                 }
                                 else
                                 {
                                    assert(argLen);
                                    running_ = false;
                                    
                                    if (rf->result_ == 0)
                                    {
                                       if (buf.get() == 0)
                                       {
                                          // must copy payload in this case
                                          *argData = new char[rf->payloadsize_];
                                          ::memcpy(*argData, buf.get(), rf->payloadsize_);
                                       }
                                       else
                                          *argData = buf.release();
                                       
                                       *argLen = rf->payloadsize_;
                                    }
                                    else
                                       throw RuntimeError(rf->result_, (const char*)buf.get(), rf->sequence_nr_);
                                 }
                                 
                                 outstandings_.erase(iter);
                              }
                              else
                                 std::cerr << "Got response for request never sent..." << std::endl;
                           }
                           break;
                        
                        case FRAME_TYPE_REGISTER_SIGNAL:
                           {
                              RegisterSignalFrame* rsf = (RegisterSignalFrame*)hdr;
                              
                              servermapid_type::iterator iter = servers_by_id_.find(rsf->serverid_);
                              if (iter != servers_by_id_.end())
                              {
                                 uint32_t registrationid = generateId();
                                 
                                 ServerSignalBase* sig = iter->second->addSignalRecipient(rsf->sig_, fds_[i].fd, registrationid, rsf->id_);
                                 
                                 if (sig)
                                    server_sighandlers_[registrationid] = sig;
                                 
                                 // FIXME error handling, what if sig is 0???
                                 SignalResponseFrame rf(registrationid, rsf->id_);
                                 rf.sequence_nr_ = rsf->sequence_nr_;
                                 
                                 genericSend(fds_[i].fd, rf, 0);
                                 
                                 if (sig)
                                    sig->onAttach(registrationid);
                              }
                              else
                                 std::cerr << "No server with id=" << rsf->serverid_ << " found." << std::endl;
                           }
                           break;
                           
                        case FRAME_TYPE_UNREGISTER_SIGNAL:
                           {
                              UnregisterSignalFrame* usf = (UnregisterSignalFrame*)hdr;
                              
                              serversignalers_type::iterator iter = server_sighandlers_.find(usf->registrationid_);
                              if (iter != server_sighandlers_.end())
                              {
                                 iter->second->removeRecipient(usf->registrationid_);
                              }
                              else
                                 std::cerr << "No registered signal '" << usf->registrationid_ << "' found." << std::endl;
                           }
                           break;
                        
                        case FRAME_TYPE_REGISTER_SIGNAL_RESPONSE:
                           {
                              SignalResponseFrame* srf = (SignalResponseFrame*)hdr;
                              
                              outstanding_signalregistrations_type::iterator iter = outstanding_sig_registrs_.find(srf->sequence_nr_);
                              if (iter != outstanding_sig_registrs_.end())
                              {
                                 sighandlers_[srf->id_] = iter->second;
                                 outstanding_sig_registrs_.erase(iter);
                              }
                              else
                                 std::cerr << "No such signal registration found." << std::endl;
                              
                              if (sequence_nr == srf->sequence_nr_)
                                 running_ = false;
                           }
                           break;

                        case FRAME_TYPE_SIGNAL:
                           {
                              SignalEmitFrame* sef = (SignalEmitFrame*)hdr;
                              
                              sighandlers_type::iterator iter = sighandlers_.find(sef->id_);
                              if (iter != sighandlers_.end())
                              {
                                 iter->second->eval(buf.get(), sef->payloadsize_);
                              }
                              else
                                 std::cerr << "No such signal handler found." << std::endl;
                           }
                           break;
                      
                        case FRAME_TYPE_RESOLVE_RESPONSE_INTERFACE:
                           {
                              InterfaceResolveResponseFrame* irrf = (InterfaceResolveResponseFrame*)hdr;
                              
                              outstanding_interface_resolves_type::iterator iter = dangling_interface_resolves_.find(irrf->sequence_nr_);
                              if (iter != dangling_interface_resolves_.end())
                              {
                                 StubBase* stub = iter->second;
                                 stub->id_ = irrf->id_;
                                 stub->current_sessionid_ = irrf->sessionid_;
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
                                 else if (sequence_nr == irrf->sequence_nr_)
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
   
   template<typename T>
   uint32_t sendInternalRequest(int fd, int func, const T& data)
   {
      Serializer s;
      s << data;
      
      RequestFrame f(RESERVED_SERVER_ID, func, INVALID_SESSION_ID);
      f.payloadsize_ = s.size();
      f.sequence_nr_ = generateSequenceNr();
   
      genericSend(fd, f, s.data());
      return f.sequence_nr_;
   }
         
   
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
         char buf[128];
         assert(strlen(stub.iface_) + 2 + strlen(stub.role_) < sizeof(buf));
         fullQualifiedName(buf, stub.iface_, stub.role_);
         
         uint32_t sequenceNr = sendInternalRequest(stub.fd_, REQUEST_RESOLVE_INTERFACE, std::string(buf));   // FIXME make this without std::string char 
         dangling_interface_resolves_[sequenceNr] = &stub;
         
         if (blockUntilResponse)
            loopUntil(sequenceNr);
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


template<template<template<typename,typename,typename> class, 
                  template<typename,typename,typename> class,
                  template<typename,typename,typename> class,
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


template<template<template<typename,typename,typename> class, 
                  template<typename,typename,typename> class,
                  template<typename,typename,typename> class,
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
   
   template<template<template<typename,typename,typename> class, 
                     template<typename,typename,typename> class,
                     template<typename,typename,typename> class,
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


template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
inline
void operator>> (ClientRequest<T1, T2, T3>& request, ClientResponse<T4, T5, T6>& response)
{
   assert(!request.handler_);
   request.handler_ = &response;
}


template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
inline
void operator>> (ServerRequest<T1, T2, T3>& request, ServerResponse<T4, T5, T6>& response)
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


template<template<typename,typename,typename> class RequestT>
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
   template<template<typename=Void,typename=Void,typename=Void> class Request, \
            template<typename=Void, typename=Void, typename=Void> class Response, \
            template<typename=Void,typename=Void,typename=Void> class Signal, \
            template<typename,typename=OnChange> class Attribute> \
      struct iface; \
            \
   template<> struct InterfaceNamer<iface<ClientRequest, ClientResponse, ClientSignal, ClientAttribute> > { static inline const char* const name() { return #  iface ; } }; \
   template<> struct InterfaceNamer<iface<ServerRequest, ServerResponse, ServerSignal, ServerAttribute> > { static inline const char* const name() { return #  iface ; } }; \
   template<template<typename=Void,typename=Void,typename=Void> class Request, \
            template<typename=Void, typename=Void, typename=Void> class Response, \
            template<typename=Void,typename=Void,typename=Void> class Signal, \
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
