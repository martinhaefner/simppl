#ifndef IPC2_H
#define IPC2_H

#include <iostream>
#include <map>
#include <set>
#include <cassert>
#include <vector>
#include <algorithm>

#include <tr1/functional>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdio>
#include <errno.h>
#include <stdint.h>
#include <memory>

#include "if.h"
#include "typetraits.h"
#include "tuple.h"
#include "calltraits.h"
#include "variant.h"   // for retrieving header sizes
#include "noninstantiable.h"

#define INVALID_SEQUENCE_NR 0xFFFFFFFF

#ifdef NDEBUG
#   define safe_cast static_cast
#else
#   define safe_cast dynamic_cast
#endif

using namespace std::tr1::placeholders;

// forward decls
template<typename IfaceT>
struct InterfaceNamer;

template<typename ServerT>
struct isServer;

struct Dispatcher;

// no forward decl, just a type definition
struct Void;


template<typename T>
struct isVoid
{
   enum { value = is_same<T, Void>::value };
};


template<typename T> 
struct isPod 
{ 
   typedef TYPELIST_14( \
      bool, \
      char, \
      signed char, \
      unsigned char, \
      short, \
      unsigned short, \
      int, \
      unsigned int, \
      long, \
      unsigned long, \
      long long, \
      unsigned long long, \
      float, \
      double) pod_types;
   
  enum { value = Find<T, pod_types>::value >= 0 }; 
};


// ------------------------------------------------------------------------------------


template<typename T>
struct isValidType
{
   enum { value = true };   // FIXME implement this in a senseful manner
};


// ------------------------------------------------------------------------------------


template<typename VariableT>
struct StackSafe
{
   typedef void(VariableT::*undofunction_type)();
   
   StackSafe(VariableT& var, undofunction_type undo)
    : var_(var)
    , undo_(undo)
   {
      // NOOP
   }
   
   ~StackSafe()
   {
      (var_.*undo_)();
   }
   
   VariableT& var_;
   undofunction_type undo_;
};


// ------------------------------------------------------------------------------------


template<typename SerT>
struct VectorSerializer
{
   inline
   VectorSerializer(SerT& s)
    : s_(s)
   {
      // NOOP
   }
   
   template<typename T>
   inline
   void operator()(const T& t)
   {
      s_.write(t);
   }
   
   SerT& s_;
};


template<typename SerializerT>
struct TupleSerializer // : noncopable
{
   TupleSerializer(SerializerT& s)
    : s_(s)
   {
      // NOOP
   }
   
   template<typename T>
   void operator()(const T& t, int /*idx*/)   // seems to be already a reference so no copy is done
   {
      s_.write(t);
   }
   
   SerializerT& s_;
};


template<typename DeserializerT>
struct TupleDeserializer // : noncopable
{
   TupleDeserializer(DeserializerT& s)
    : s_(s)
   {
      // NOOP
   }
   
   template<typename T>
   void operator()(T& t, int /*idx*/)
   {
      s_.read(t);
   }
   
   DeserializerT& s_;
};


// ------------------------------------------------------------------------------------


struct Serializer // : noncopyable
{
   inline
   Serializer(size_t initial = 64)
    : capacity_(initial)
    , buf_(capacity_ > 0 ? (char*)::malloc(capacity_) : 0)
    , current_(buf_)
   {
      // NOOP
   }
   
   inline
   ~Serializer()
   {
      free(buf_);
   }
 
   static inline
   void free(void* buf)
   {
      ::free(buf);
   }
   
   /// any subsequent calls to the object are invalid
   inline
   void* release()
   {
      void* rc = buf_;
      buf_ = 0;
      return rc;
   }
   
   inline
   size_t size() const
   {
      if (capacity_ > 0)
      {
         assert(buf_);
         return current_ - buf_;
      }
      else
         return 0;
   }
   
   inline
   const void* data() const
   {
      return buf_;
   }
   
   template<typename T>
   inline
   Serializer& write(const T& t)
   {
      return write(t, bool_<isPod<T>::value>());
   }

   template<typename T>
   inline
   Serializer& write(T t, tTrueType)
   {
      enlarge(sizeof(T));
      
      memcpy(current_, &t, sizeof(T));
      current_ += sizeof(T);
      
      return *this;
   }
   
   template<typename T>
   inline
   Serializer& write(const T& t, tFalseType)
   {
      return write(*(const typename T::type*)&t);
   }
   
   Serializer& write(const char* str)
   {
      int32_t len = strlen(str) + 1;
      enlarge(len+sizeof(len));
      
      memcpy(current_, &len, sizeof(len));
      current_  += sizeof(len);
      memcpy(current_, str, len);
      current_ += len;
      
      return *this;
   }
   
   Serializer& write(const std::string& str)
   {
      int32_t len = str.size() + 1;
      enlarge(len+sizeof(len));
      
      memcpy(current_, &len, sizeof(len));
      current_  += sizeof(len);
      memcpy(current_, str.c_str(), len);
      current_ += len;
      
      return *this;
   }
   
   template<typename T>
   inline
   Serializer& write(const std::vector<T>& v)
   {
      int32_t len = v.size();
      enlarge(len*sizeof(T) + sizeof(len));
 
      memcpy(current_, &len, sizeof(len));
      current_ += sizeof(len);
 
      std::for_each(v.begin(), v.end(), VectorSerializer<Serializer>(*this));
            
      return *this;
   }
   
   template<typename KeyT, typename ValueT>
   Serializer& write(const std::map<KeyT, ValueT>& m)
   {
      int32_t len = m.size();
      enlarge(len*(sizeof(KeyT) + sizeof(ValueT)) + sizeof(len));
      
      memcpy(current_, &len, sizeof(len));
      current_ += sizeof(len);
 
      std::for_each(m.begin(), m.end(), std::tr1::bind(&Serializer::write<KeyT, ValueT>, this, _1));

      return *this;
   }
   
   template<typename T1, typename T2, typename T3, typename T4>
   Serializer& write(const Tuple<T1, T2, T3, T4>& tuple)
   {
      for_each(tuple, TupleSerializer<Serializer>(*this));
      return *this;
   }
   
private:
   
   template<typename KeyT, typename ValueT>
   inline
   void write(const std::pair<KeyT, ValueT>& p)
   {
      write(p.first).write(p.second);
   }
   
   void enlarge(size_t needed)
   {
      size_t current = size();
      size_t estimated_capacity = current + needed;
      
      if (capacity_ < estimated_capacity)
      {
         while (capacity_ < estimated_capacity)
            capacity_ <<=1 ;
         
         buf_ = (char*)::realloc(buf_, capacity_);
         current_ = buf_ + current;
      }
   }
   
   size_t capacity_;
   
   char* buf_;
   char* current_;
};


#define MAKE_SERIALIZER(type) \
inline \
Serializer& operator<<(Serializer& s, type t) \
{ \
   return s.write(t); \
}

MAKE_SERIALIZER(bool)

MAKE_SERIALIZER(unsigned short)
MAKE_SERIALIZER(short)

MAKE_SERIALIZER(unsigned int)
MAKE_SERIALIZER(int)

MAKE_SERIALIZER(unsigned long)
MAKE_SERIALIZER(long)

MAKE_SERIALIZER(unsigned long long)
MAKE_SERIALIZER(long long)

MAKE_SERIALIZER(double)
MAKE_SERIALIZER(const char*)


inline 
Serializer& operator<<(Serializer& s, const std::string& str) 
{ 
   return s.write(str); 
}

template<typename T>
inline 
Serializer& operator<<(Serializer& s, const std::vector<T>& v) 
{ 
   return s.write(v); 
}

template<typename KeyT, typename ValueT>
inline 
Serializer& operator<<(Serializer& s, const std::map<KeyT, ValueT>& m) 
{ 
   return s.write(m); 
}

template<typename StructT>
inline
Serializer& operator<<(Serializer& s, const StructT& st)
{
   const typename StructT::type& tuple = *(const typename StructT::type*)&st;
   return s.write(tuple);
}

template<typename T1, typename T2, typename T3, typename T4>
inline
Serializer& operator<<(Serializer& s, const Tuple<T1, T2, T3, T4>& t)
{
   return s.write(t);
}


// -----------------------------------------------------------------------------


struct Deserializer // : noncopyable
{
   static inline
   void free(char* ptr)
   {
      delete[] ptr;
   }
   
   inline
   Deserializer(const void* buf, size_t capacity)
    : capacity_(capacity)
    , buf_((const char*)buf)
    , current_(buf_)
   {
      // NOOP
   }
   
   inline
   size_t size() const
   {
      return current_ - buf_;
   }
   
   template<typename T>
   Deserializer& read(T& t)
   {
      memcpy(&t, current_, sizeof(T));
      current_ += sizeof(T);
      
      return *this;
   }
   
   Deserializer& read(char*& str)
   {
      assert(str == 0);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free
      
      uint32_t len;
      read(len);
      
      // FIXME how is len referenced in string functions (including or excluding the last 0)
      if (len > 0)
      {
         str = allocate(len);
         memcpy(str, current_, len);
         current_ += len;
      }
      else
      {
         str = allocate(1);
         *str = '\0';
      }
      
      return *this;
   }
   
   Deserializer& read(std::string& str)
   {
      uint32_t len;
      read(len);
      
      if (len > 0)
      {
         str.assign(current_, len);
         current_  += len;
      }
      else
         str.clear();
   
      return *this;
   }
   
   template<typename T>
   Deserializer& read(std::vector<T>& v)
   {
      uint32_t len;
      read(len);
      
      if (len > 0)
      {
         v.resize(len);
         
         for(uint32_t i=0; i<len; ++i)
         {
            //read(v[i]);
            *this >> v[i];
         }
      }
      else
         v.clear();
   
      return *this;
   }
   
   template<typename KeyT, typename ValueT>
   Deserializer& read(std::map<KeyT, ValueT>& m)
   {
      uint32_t len;
      read(len);
      
      if (len > 0)
      {
         for(uint32_t i=0; i<len; ++i)
         {
            // FIXME does not work as expected
            std::pair<KeyT, ValueT> p;
            read(p.first).read(p.second);
            m.insert(p);
         }
      }
      else
         m.clear();
   
      return *this;
   }
   
   template<typename T1, typename T2, typename T3, typename T4>
   Deserializer& read(Tuple<T1, T2, T3, T4>& tuple)
   {
      for_each(tuple, TupleDeserializer<Deserializer>(*this));
      return *this;
   }
   
private:
   
   static inline
   char* allocate(size_t len)
   {
      return new char[len];
   }
   
   const size_t capacity_;
   
   const char* buf_;
   const char* current_;
};


#define MAKE_DESERIALIZER(type) \
inline \
Deserializer& operator>>(Deserializer& s, type& t) \
{ \
   return s.read(t); \
}

MAKE_DESERIALIZER(bool)

MAKE_DESERIALIZER(unsigned short)
MAKE_DESERIALIZER(short)

MAKE_DESERIALIZER(unsigned int)
MAKE_DESERIALIZER(int)

MAKE_DESERIALIZER(unsigned long)
MAKE_DESERIALIZER(long)

MAKE_DESERIALIZER(unsigned long long)
MAKE_DESERIALIZER(long long)

MAKE_DESERIALIZER(double)
MAKE_DESERIALIZER(const char*)


inline 
Deserializer& operator>>(Deserializer& s, std::string& str) 
{ 
   return s.read(str); 
}

template<typename T>
inline 
Deserializer& operator>>(Deserializer& s, std::vector<T>& v) 
{ 
   return s.read(v); 
}

template<typename KeyT, typename ValueT>
inline 
Deserializer& operator>>(Deserializer& s, std::map<KeyT, ValueT>& m) 
{ 
   return s.read(m); 
}

template<typename StructT>
inline
Deserializer& operator>>(Deserializer& s, StructT& st)
{
   typename StructT::type& tuple = *(typename StructT::type*)&st;
   return s.read(tuple);
}

template<typename T1, typename T2, typename T3, typename T4>
inline
Deserializer& operator>>(Deserializer& s, Tuple<T1, T2, T3, T4>& t)
{
   return s.read(t);
}


// -----------------------------------------------------------------------------------------


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
};


// -----------------------------------------------------------------------------


struct ServerRequestBase
{
   friend struct ServerRequestBaseSetter;
   
   virtual void eval(void* payload, size_t length) = 0;

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
#define FRAME_TYPE_NAMED_REQUEST               0x5u
#define FRAME_TYPE_REGISTER_SIGNAL             0x6u
#define FRAME_TYPE_UNREGISTER_SIGNAL           0x7u
#define FRAME_TYPE_SIGNAL                      0x8u
#define FRAME_TYPE_REGISTER_SIGNAL_RESPONSE    0x9u


struct FrameHeader
{
   inline
   FrameHeader()
    : magic_(0)
    , type_(0)
    , payloadsize_(0)
    , sequence_nr_(0)
   {
      // NOOP
   }
   
   inline
   FrameHeader(uint32_t type)
    : magic_(FRAME_MAGIC)
    , type_(type)
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
   uint32_t type_;   
   
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
    , id_(0)
   {
      // NOOP
   }
   
   inline
   InterfaceResolveResponseFrame(uint32_t id)
    : FrameHeader(FRAME_TYPE_RESOLVE_RESPONSE_INTERFACE)
    , id_(id)
   {
      // NOOP
   }
   
   uint32_t id_;     // invalid id is 0.
};


struct NamedRequestFrame : FrameHeader
{
   inline
   NamedRequestFrame()
    : FrameHeader()
   {
      memset(this, 0, sizeof(*this));
   }
   
   inline
   NamedRequestFrame(const char* iface, const char* role, uint32_t func)
    : FrameHeader(FRAME_TYPE_NAMED_REQUEST)
    , func_(func)
   {
      assert(strlen(iface) < sizeof(iface_));
      assert(strlen(role) < sizeof(role_));
      
      strcpy(iface_, iface);
      strcpy(role_, role);
   }
   
   char iface_[28];
   char role_[16];
   uint32_t func_;
};


struct RequestFrame : FrameHeader
{
   inline
   RequestFrame()
    : FrameHeader()
    , serverid_(0)
    , func_(0)
   {
      // NOOP
   }
   
   inline
   RequestFrame(uint32_t serverid, uint32_t func)
    : FrameHeader(FRAME_TYPE_REQUEST)
    , serverid_(serverid)
    , func_(func)
   {
      // NOOP
   }
   
   uint32_t serverid_;
   uint32_t func_;
};


struct ResponseFrame : FrameHeader
{
   inline
   ResponseFrame()
    : FrameHeader()
    , result_(0)
    , reserved_(0)
   {
      // NOOP
   }
   
   inline
   ResponseFrame(int32_t result)
    : FrameHeader(FRAME_TYPE_RESPONSE)
    , result_(result)
    , reserved_(0)
   {
      // NOOP
   }
   
   int32_t result_;     // 0 = ok result, any other value: exceptional result, exception text may be added as payload
   int32_t reserved_;
};


struct RegisterSignalFrame : FrameHeader
{
   inline
   RegisterSignalFrame()
    : FrameHeader()
   {
      memset(this, 0, sizeof(*this));
   }
   
   inline
   RegisterSignalFrame(const char* iface, const char* role, uint32_t sig)
    : FrameHeader(FRAME_TYPE_REGISTER_SIGNAL)
    , sig_(sig)
   {
      assert(strlen(iface) < sizeof(iface_));
      assert(strlen(role) < sizeof(role_));
      
      strcpy(iface_, iface);
      strcpy(role_, role);
   }
   
   char iface_[28];
   char role_[16];
   uint32_t sig_;
   uint32_t reserved_;
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
   uint32_t reserved_;
};


struct SignalEmitFrame : FrameHeader
{
   inline
   SignalEmitFrame()
    : FrameHeader()
    , registrationid_(0)
    , reserved_(0)
   {
      // NOOP
   }
   
   inline
   SignalEmitFrame(uint32_t registrationid)
    : FrameHeader(FRAME_TYPE_SIGNAL)
    , registrationid_(registrationid)
    , reserved_(0)
   {
      // NOOP
   }
   
   uint32_t registrationid_;
   uint32_t reserved_;
};


struct SignalResponseFrame : FrameHeader
{
   inline
   SignalResponseFrame()
    : FrameHeader()
    , registrationid_(0)
    , reserved_(0)
   {
      // NOOP
   }
   
   inline
   SignalResponseFrame(uint32_t registrationid)
    : FrameHeader(FRAME_TYPE_REGISTER_SIGNAL_RESPONSE)
    , registrationid_(registrationid)
    , reserved_(0)
   {
      // NOOP
   }
   
   uint32_t registrationid_; 
   uint32_t reserved_;
};


// ------------------------------------------------------------------------------------------


struct HeaderSize
{
   typedef TYPELIST_9(InterfaceResolveFrame, InterfaceResolveResponseFrame, RequestFrame, NamedRequestFrame, ResponseFrame, RegisterSignalFrame, UnregisterSignalFrame, SignalEmitFrame, SignalResponseFrame) headertypes;

   enum { max = Max<headertypes, SizeFunc>::value };
   
   // FIXME make this part of typelist algorithms
   static const unsigned int size[10];
};


/*static*/ 
const unsigned int HeaderSize::size[10] = {
   0
 , sizeof(RequestFrame)
 , sizeof(ResponseFrame)
 , sizeof(InterfaceResolveFrame)
 , sizeof(InterfaceResolveResponseFrame) 
 , sizeof(NamedRequestFrame)
 , sizeof(RegisterSignalFrame)
 , sizeof(UnregisterSignalFrame)
 , sizeof(SignalEmitFrame)
 , sizeof(SignalResponseFrame)
};

STATIC_CHECK((sizeof(HeaderSize::size) / sizeof(HeaderSize::size[0])) == Size<HeaderSize::headertypes>::value + 1, error_frame_types_must_occur_here);


// -------------------------------------------------------------------------------------------


template<typename FrameT>
bool genericSend(int fd, const FrameT& f, const void* payload)
{
   iovec iov[2] = { { const_cast<FrameT*>(&f), sizeof(FrameT) }, { const_cast<void*>(payload), f.payloadsize_ } };
   
   msghdr msg;
   memset(&msg, 0, sizeof(msg));
   msg.msg_iov = iov;
   msg.msg_iovlen = payload ? 2 : 1;
   
   if (::sendmsg(fd, &msg, MSG_NOSIGNAL) < 0)
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
   int connect(bool block);
   
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
   
   virtual void connected()
   {
      // NOOP
   }
   
   StubBase(const char* iface, const char* role, const char* boundname)
    : iface_(iface) 
    , role_(role)
    , id_(0)
    , disp_(0)
    , fd_(-1)
   {
      assert(iface);
      assert(role);
      assert(boundname && strlen(boundname) < sizeof(boundname_));
      strcpy(boundname_, boundname);
   }
   
protected:
   
   uint32_t sendRequest(ClientResponseBase* handler, uint32_t requestid, const Serializer& s);
   
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
   
   char boundname_[24];
   uint32_t id_;            ///< as given from server
   
   Dispatcher* disp_;
   int fd_;                 ///< connected socket
};


// -------------------------------------------------------------------------------------------


struct ClientResponseBase
{
   virtual void eval(const void* payload, size_t len) = 0;
   
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
            typename if_<isVoid<T1>::value, std::tr1::function<void()>, 
               /*else*/std::tr1::function<void(arg1_type)> >::type,    
            /*else*/std::tr1::function<void(arg1_type, arg2_type)> >::type, 
         /*else*/std::tr1::function<void(arg1_type, arg2_type, arg3_type)> >::type 
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
   
   /// send registration to the server
   inline
   ClientSignal& attach()
   {
      parent<StubBase>()->sendSignalRegistration(*this);
      return *this;
   }
   
   /// send de-registration to the server
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
   void operator()(const std::pair<uint32_t, int>& info)
   {
      SignalEmitFrame f(info.first);
      f.payloadsize_ = len_;
      
      // FIXME need to remove socket on EPIPE 
      genericSend(info.second, f, data_);
   }
   
   const void* data_;
   size_t len_;
};


struct ServerSignalBase
{
   inline
   void sendSignal(const Serializer& s)
   {
      std::for_each(recipients_.begin(), recipients_.end(), SignalSender(s.data(), s.size()));
   }
   
   inline
   void addRecipient(int fd, uint32_t registrationid)
   {
      recipients_[registrationid] = fd;
   }

   inline
   void removeRecipient(uint32_t registrationid)
   {
      recipients_.erase(registrationid);
   }
   
protected:
   
   inline
   ~ServerSignalBase()
   {
      // NOOP
   }
   
   std::map<uint32_t/*registrationid*/, int/*fd*/> recipients_;
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
      STATIC_CHECK(isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
      
      static Serializer s(0);
      sendSignal(s);
   }
   
   inline
   void emit(arg1_type arg1)
   {
      STATIC_CHECK(!isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
    
      Serializer s(sizeof(T1));
      sendSignal(s << arg1);
   }
   
   inline
   void emit(arg1_type arg1, arg2_type arg2)
   {
      STATIC_CHECK(!isVoid<T1>::value && !isVoid<T2>::value && isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
    
      Serializer s(sizeof(T1) + sizeof(T2));
      sendSignal(s << arg1 << arg2);
   }
   
   inline
   void emit(arg1_type arg1, arg2_type arg2, arg3_type arg3)
   {
      STATIC_CHECK(!isVoid<T1>::value && !isVoid<T2>::value && !isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
    
      Serializer s(sizeof(T1) + sizeof(T2) + sizeof(T3));
      sendSignal(s << arg1 << arg2 << arg3);
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
      STATIC_CHECK(isValidType<T1>::value && isValidType<T2>::value && isValidType<T3>::value, invalid_type_in_interface);
      parent.push_back(this);
   }
   
   inline
   ClientResponseHolder operator()()
   {
      STATIC_CHECK(isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
      
      static Serializer s(0);
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(handler_, id_, s));
   }
   
   inline
   ClientResponseHolder operator()(arg1_type t1)
   {
      STATIC_CHECK(!isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
      
      Serializer s(sizeof(typename remove_ref<T1>::type));
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(handler_, id_, s << t1));
   }

   inline
   ClientResponseHolder operator()(arg1_type t1, arg2_type t2)
   {
      STATIC_CHECK(!isVoid<T1>::value && !isVoid<T2>::value && isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
      
      Serializer s(sizeof(typename remove_ref<T1>::type) + sizeof(typename remove_ref<T2>::type));
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(handler_, id_, s << t1 << t2));
   }
   
   inline
   ClientResponseHolder operator()(arg1_type t1, arg2_type t2, arg3_type t3)
   {
      STATIC_CHECK(!isVoid<T1>::value && !isVoid<T2>::value && !isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
      
      Serializer s(sizeof(typename remove_ref<T1>::type) + sizeof(typename remove_ref<T2>::type) + sizeof(typename remove_ref<T3>::type));
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(handler_, id_, s << t1 << t2 << t3));
   }
   
   ClientResponseBase* handler_;
   uint32_t id_;
};


// --------------------------------------------------------------------------------------------


template<typename T1, typename T2, typename T3>
struct ClientResponse : ClientResponseBase
{
   typedef typename CallTraits<T1>::param_type arg1_type;
   typedef typename CallTraits<T2>::param_type arg2_type;
   typedef typename CallTraits<T3>::param_type arg3_type;
   
   typedef 
      typename if_<isVoid<T3>::value, 
         typename if_<isVoid<T2>::value, 
            typename if_<isVoid<T1>::value, std::tr1::function<void()>, 
               /*else*/std::tr1::function<void(arg1_type)> >::type,    
            /*else*/std::tr1::function<void(arg1_type, arg2_type)> >::type, 
         /*else*/std::tr1::function<void(arg1_type, arg2_type, arg3_type)> >::type 
      function_type;
   
   inline
   ClientResponse()
   {
      STATIC_CHECK(isValidType<T1>::value && isValidType<T2>::value && isValidType<T3>::value, invalid_type_in_interface);
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
         std::cerr << "No appropriate handler registered for response with payload size=" << length << std::endl;
   }
   
   function_type f_;
};


template<typename T1, typename T2, typename T3, typename FunctorT>
inline 
void operator>> (ClientResponse<T1, T2, T3>& r, const FunctorT& f)
{
   r.handledBy(f);
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
            typename if_<isVoid<T1>::value, std::tr1::function<void()>, 
               /*else*/std::tr1::function<void(arg1_type)> >::type,    
            /*else*/std::tr1::function<void(arg1_type, arg2_type)> >::type, 
         /*else*/std::tr1::function<void(arg1_type, arg2_type, arg3_type)> >::type 
      function_type;
   
   inline
   ServerRequest(uint32_t id, std::map<uint32_t, ServerRequestBase*>& requests)
   {
      STATIC_CHECK(isValidType<T1>::value && isValidType<T2>::value && isValidType<T3>::value, invalid_type_in_interface);
      requests[id] = this;
   }
   
   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      assert(!f_);
      f_ = func;
   }
      
   void eval(void* payload, size_t length)
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
      STATIC_CHECK(isValidType<T1>::value && isValidType<T2>::value && isValidType<T3>::value, invalid_type_in_interface);
   }
   
   inline
   ServerResponseHolder operator()(arg1_type t1)
   {
      STATIC_CHECK(!isVoid<T1>::value && isVoid<T2>::value && isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
      
      Serializer s(sizeof(T1));      
      return ServerResponseHolder(s << t1, *this);
   }
   
   inline
   ServerResponseHolder operator()(arg1_type t1, arg2_type t2)
   {
      STATIC_CHECK(!isVoid<T1>::value && !isVoid<T2>::value && isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
      
      Serializer s(sizeof(T1) + sizeof(T2));      
      return ServerResponseHolder(s << t1 << t2, *this);
   }
   
   inline
   ServerResponseHolder operator()(arg1_type t1, arg2_type t2, arg3_type t3)
   {
      STATIC_CHECK(!isVoid<T1>::value && !isVoid<T2>::value && !isVoid<T3>::value, invalid_function_call_due_to_arguments_mismatch);
      
      Serializer s(sizeof(T1) + sizeof(T2) + sizeof(T3));      
      return ServerResponseHolder(s << t1 << t2 << t3, *this);
   }
};


typedef void(*caller_func_type)(void*, const NamedRequestFrame&, int, void*, size_t);

struct ServerHolderBase
{
   virtual ~ServerHolderBase()
   {
      // NOOP
   }
   
   virtual void eval(uint32_t funcid, uint32_t sequence_nr, int fd, void* payload, size_t len) = 0;
   
   virtual ServerSignalBase* addSignalRecipient(uint32_t id, int fd, uint32_t registrationid) = 0;
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
   void eval(uint32_t funcid, uint32_t sequence_nr, int fd, void* payload, size_t len)
   {
      handler_->handleRequest(funcid, sequence_nr, fd, payload, len);
   }
   
   /*virtual*/
   ServerSignalBase* addSignalRecipient(uint32_t id, int fd, uint32_t registrationid)
   {
      ServerSignalBase* rc = 0;
      
      std::map<uint32_t, ServerSignalBase*>::iterator iter = handler_->signals_.find(id);
      if (iter != handler_->signals_.end())
      {
         iter->second->addRecipient(fd, registrationid);
         rc = iter->second;
      }
      
      return rc;
   }
   
   SkeletonT* handler_;
};
   

struct Dispatcher
{
   friend struct StubBase;
   
   typedef std::map<std::string, ServerHolderBase*> servermap_type;
   typedef std::map<uint32_t, ServerHolderBase*> servermapid_type;
   typedef std::multimap<std::string, StubBase*> clientmap_type;
   typedef std::map<uint32_t, StubBase*> clientmapid_type;
   typedef std::map<uint32_t, ClientResponseBase*> outstanding_requests_type;
   typedef std::map<uint32_t, ClientSignalBase*> outstanding_signalregistrations_type;
   typedef std::map<uint32_t, ClientSignalBase*> sighandlers_type;   // FIXME BUG the registrationid (index of this container) is not world unique
   typedef std::map<uint32_t, ServerSignalBase*> serversignalers_type;
   
   Dispatcher(const char* boundname = 0)
    : running_(false)
    , acceptor_(-1)
    , sequence_(0)
    , nextserverid_(0)
   {
      ::memset(fds_, 0, sizeof(fds_));
      
      if (boundname)
      {
         ::mkdir("/tmp/dispatcher", 0777);
         
         acceptor_ = ::socket(PF_UNIX, SOCK_STREAM, 0);
         
         struct sockaddr_un addr;
         addr.sun_family = AF_UNIX;
         sprintf(addr.sun_path, "/tmp/dispatcher/%s", boundname);
         
         ::unlink(addr.sun_path);         
         ::bind(acceptor_, (struct sockaddr*)&addr, sizeof(addr));
         
         ::listen(acceptor_, 16);
         
         fds_[0].fd = acceptor_;
         fds_[0].events = POLLIN;
      }
   }
   
   inline
   ~Dispatcher()
   {
      for(servermap_type::iterator iter = servers_.begin(); iter != servers_.end(); ++iter)
      {
         delete iter->second;
      }
   }
   
   template<typename ServerT>
   void addServer(ServerT& serv)
   {
      STATIC_CHECK(isServer<ServerT>::value, only_add_servers_here);
      
      std::string name = InterfaceNamer<typename ServerT::interface_type>::name();
      name += "::";
      name += serv.role_;
    
      assert(servers_.find(name) == servers_.end());
      
      std::cout << "Adding server for '" << name << "'" << std::endl;
      ServerHolder<ServerT>* holder = new ServerHolder<ServerT>(serv);
      servers_[name] = holder;
      servers_by_id_[++nextserverid_] = holder;
   }
   
   inline
   void addRequest(ClientResponseBase& r, uint32_t sequence_nr)
   {
      outstandings_[sequence_nr] = &r;
   }
   
   inline
   bool addSignalRegistration(ClientSignalBase& s, uint32_t sequence_nr)
   {
      bool alreadyAttached = false;
      
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
      return ++sequence_;
   }
   
   void addClient(StubBase& clnt)
   {
      // FIXME if added if the event loop is already running we have to connect the client here...
      assert(!clnt.disp_);
      
      clnt.disp_ = this;
      clients_.insert(std::make_pair(clnt.boundname_, &clnt)); 
   }
   
   template<typename T1>
   bool waitForResponse(const ClientResponseHolder& resp, T1& t1)
   {
      assert(resp.r_);
      assert(!running_);
      
      void* data = 0;
      size_t len = 0;
      
      int rc = loopUntil(resp.sequence_nr_, &data, &len);
      
      if (rc == 0)
      {
         ClientResponse<T1, Void, Void>* r = safe_cast<ClientResponse<T1, Void, Void>*>(resp.r_ );
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
      
      void* data = 0;
      size_t len = 0;
      
      int rc = loopUntil(resp.sequence_nr_, &data, &len);
      
      if (rc == 0)
      {
         ClientResponse<T1, T2, Void>* r = safe_cast<ClientResponse<T1, T2, Void>*>(resp.r_ );
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
      
      void* data = 0;
      size_t len = 0;
      
      int rc = loopUntil(resp.sequence_nr_, &data, &len);
      
      if (rc == 0)
      {
         ClientResponse<T1, T2, T3>* r = safe_cast<ClientResponse<T1, T2, T3>*>(resp.r_ );
         assert(r);
         
         Deserializer d(data, len);
         d >> t1 >> t2 >> t3;
      }
      
      return rc == 0;
   }

   // FIXME private internal
   // FIXME INVALID_SEQUENCE_NR is not a good idea. Must think about this. Maybe we should make the sequence_nr an uint64_t
   // or use special counters for registrations since an overflow is not good in this case!
   int loopUntil(uint32_t sequence_nr = INVALID_SEQUENCE_NR, void** argData = 0, size_t* argLen = 0, unsigned int timeoutMs = 2000)
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
   
   int once(unsigned int timeoutMs = 2000)
   {
      return once_(INVALID_SEQUENCE_NR, 0, 0, timeoutMs);
   }
   
private:
   
   int once_(uint32_t sequence_nr, void** argData, size_t* argLen, unsigned int timeoutMs)
   {
      int retval = 0;
      
      int rc = poll(fds_, sizeof(fds_)/sizeof(fds_[0]), timeoutMs);
            
      if (rc > 0)
      {
         if (fds_[0].revents & POLLIN)
         {
            struct sockaddr_un addr;
            socklen_t len = sizeof(addr);
      
            int fd = accept(acceptor_, (struct sockaddr*)&addr, &len);
            if (fd > 0)
            {
               fds_[fd].fd = fd;
               fds_[fd].events = POLLIN;
            }
         }
         else
         {
            for(unsigned int i=1; i<sizeof(fds_)/sizeof(fds_[0]); ++i)
            {
               if (fds_[i].revents & POLLIN)
               {
                  FrameHeader f;
                     
                  ssize_t len = ::recv(fds_[i].fd, &f, sizeof(f), MSG_NOSIGNAL|MSG_WAITALL|MSG_PEEK);
                  
                  if (len == sizeof(f) && f)
                  {
                     len = 0;
                     std::auto_ptr<char> buf(0);   // it's safe since POD array though scoped_array would be better here!
                     
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
                        case FRAME_TYPE_NAMED_REQUEST:
                           {
                              NamedRequestFrame* nrf = (NamedRequestFrame*)hdr;
                              
                              std::string name(nrf->iface_);
                              name += "::";
                              name += nrf->role_;
                              
                              servermap_type::iterator iter = servers_.find(name);
                              if (iter != servers_.end())
                              {
                                 iter->second->eval(nrf->func_, nrf->sequence_nr_, fds_[i].fd, buf.get(), nrf->payloadsize_);
                              }
                              else
                                 std::cerr << "No service '" << name << "' found." << std::endl;
                           }
                           break;
                        
                        case FRAME_TYPE_REQUEST:
                           {
                              RequestFrame* rf = (RequestFrame*)hdr;
                              
                              servermapid_type::iterator iter = servers_by_id_.find(rf->serverid_);
                              if (iter != servers_by_id_.end())
                              {
                                 iter->second->eval(rf->func_, rf->sequence_nr_, fds_[i].fd, buf.get(), rf->payloadsize_);
                              }
                              else
                                 std::cerr << "No service with id=" << rf->serverid_ << " found." << std::endl;
                           }
                           break;
                           
                        case FRAME_TYPE_RESPONSE:
                           {
                              ResponseFrame* rf = (ResponseFrame*)hdr;
                              
                              outstanding_requests_type::iterator iter;
                              if ((iter = outstandings_.find(rf->sequence_nr_)) != outstandings_.end())
                              {
                                 // FIXME that stuff with the sequence nr does not yet fit...
                                 if (sequence_nr == INVALID_SEQUENCE_NR)
                                 {
                                    iter->second->eval(buf.get(), rf->payloadsize_);
                                 }
                                 else
                                 {
                                    if (rf->sequence_nr_ == sequence_nr)
                                    {
                                       assert(argData && argLen);
                                    
                                       *argData = buf.release();
                                       *argLen = rf->payloadsize_;
                                    }
                                       running_ = false;
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
                              
                              std::string name(rsf->iface_);
                              name += "::";
                              name += rsf->role_;
                              
                              servermap_type::iterator iter = servers_.find(name);
                              if (iter != servers_.end())
                              {
                                 uint32_t registrationid = generateSequenceNr();
                                 
                                 ServerSignalBase* sig = iter->second->addSignalRecipient(rsf->sig_, fds_[i].fd, registrationid);
                                 
                                 if (sig)
                                    server_sighandlers_[registrationid] = sig;
                                 
                                 SignalResponseFrame rf(registrationid);
                                 rf.sequence_nr_ = rsf->sequence_nr_;
                                 
                                 genericSend(fds_[i].fd, rf, 0);
                              }
                              else
                                 std::cerr << "No server '" << name << "' found." << std::endl;
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
                                 sighandlers_[srf->registrationid_] = iter->second;
                                 outstanding_sig_registrs_.erase(iter);
                              }
                              else
                                 std::cerr << "No such signal registration found." << std::endl;
                              
                              if (sequence_nr == srf->sequence_nr_)
                              {   
                                 running_ = false;
                              }
                           }
                           break;

                        case FRAME_TYPE_SIGNAL:
                           {
                              SignalEmitFrame* sef = (SignalEmitFrame*)hdr;
                              
                              sighandlers_type::iterator iter = sighandlers_.find(sef->registrationid_);
                              if (iter != sighandlers_.end())
                              {
                                 iter->second->eval(buf.get(), sef->payloadsize_);
                              }
                              else
                                 std::cerr << "No such signal handler found." << std::endl;
                           }
                           break;
                        
                        case FRAME_TYPE_RESOLVE_INTERFACE:
                           {
                              InterfaceResolveFrame* irf = (InterfaceResolveFrame*)hdr;
                              
                              InterfaceResolveResponseFrame rf(0);
                              rf.sequence_nr_ = irf->sequence_nr_;
                                 
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
                              InterfaceResolveResponseFrame* irrf = (InterfaceResolveResponseFrame*)hdr;
                              
                              clientmapid_type::iterator iter = dangling_interface_resolves_.find(irrf->sequence_nr_);
                              if (iter != dangling_interface_resolves_.end())
                              {
                                 StubBase* stub = iter->second;
                                 stub->id_ = irrf->id_;
                                 dangling_interface_resolves_.erase(iter);
                                 
                                 if (sequence_nr == INVALID_SEQUENCE_NR)
                                 {
                                    // eventloop driven
                                    for (clientmap_type::iterator it = clients_.begin(); it != clients_.end(); ++it)
                                    {
                                       if (!strcmp(stub->boundname_, it->first.c_str()))
                                       {
                                          it->second->connected();
                                       }
                                    } 
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
      
      // FIXME whole connection algorithm is broken?!
      for(clientmap_type::iterator iter = clients_.begin(); iter != clients_.end(); ++iter)
      {
         if (iter->second->fd_ <= 0)
         {
            std::string current = iter->first;
            
            int fd = connect(*iter->second);
            
            if (fd > 0)
            {
               iter->second->fd_ = fd;
               
               ++iter;
               while(iter != clients_.end() && iter->first == current)
               {
                  iter->second->fd_ = fd;
                  ++iter;
               }
               --iter;
            }
            // else FIXME maybe we could initialize asynchronous connect here
         }
      }
      
      loopUntil();
   }
   
   void clearSlot(int idx)
   {
      while(::close(fds_[idx].fd) && errno == EINTR);
                        
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
   
private:
   
   // FIXME first look in dispatchers connected sockets...
   int connect(StubBase& stub, bool blockUntilResponse = false)
   {
      int fd = socket(PF_UNIX, SOCK_STREAM, 0);
      
      struct sockaddr_un addr;
      addr.sun_family = AF_UNIX;
      sprintf(addr.sun_path, "/tmp/dispatcher/%s", stub.boundname_);
      
      int rc = ::connect(fd, (struct sockaddr*)&addr, sizeof(addr));
      
      if (rc < 0)
      {
         ::close(fd);
         return -1;
      }
      else
      {
         fds_[fd].fd = fd;
         fds_[fd].events = POLLIN;
         
         InterfaceResolveFrame f(42);
         char buf[128];
         
         assert(strlen(stub.iface_) + 2 + strlen(stub.role_) < sizeof(buf));
         strcpy(buf, stub.iface_);
         strcat(buf, "::");
         strcat(buf, stub.role_);
                                 
         f.payloadsize_ = strlen(buf)+1;
         f.sequence_nr_ = generateSequenceNr();
   
         dangling_interface_resolves_[f.sequence_nr_] = &stub;
         genericSend(fd, f, buf);
         
         if (blockUntilResponse)
            loopUntil(f.sequence_nr_);
         
         return fd;
      }
   }
   
   //registered servers
   servermap_type servers_;
   servermapid_type servers_by_id_;
   clientmapid_type dangling_interface_resolves_;
   
   uint32_t nextserverid_;
   bool running_;
   
   // incoming acceptor socket
   int acceptor_;
   
   pollfd fds_[32];
   
   // registered clients
   clientmap_type clients_;
   
   uint32_t sequence_;
   
   outstanding_requests_type outstandings_;
   outstanding_signalregistrations_type outstanding_sig_registrs_;
   sighandlers_type sighandlers_;
   serversignalers_type server_sighandlers_;
};


uint32_t StubBase::sendRequest(ClientResponseBase* handler, uint32_t id, const Serializer& s)
{
   if (id_)
   {
      RequestFrame f(id_, id);
      f.payloadsize_ = s.size();
      f.sequence_nr_ = disp_->generateSequenceNr();
      
      if (genericSend(fd(), f, s.data()))
      {
         if (handler)
            disp_->addRequest(*handler, f.sequence_nr_);
      }
      return f.sequence_nr_;
   }
   else
   {
      NamedRequestFrame f(iface_, role_, id);
      f.payloadsize_ = s.size();
      f.sequence_nr_ = disp_->generateSequenceNr();
      
      if (genericSend(fd(), f, s.data()))
      {
         if (handler)
            disp_->addRequest(*handler, f.sequence_nr_);
      }
      return f.sequence_nr_;
   }
}


void StubBase::sendSignalRegistration(ClientSignalBase& sigbase)
{
   assert(disp_);
   
   RegisterSignalFrame f(iface_, role_, sigbase.id());
   f.payloadsize_ = 0;
   f.sequence_nr_ = disp_->generateSequenceNr();
   
   if (disp_->addSignalRegistration(sigbase, f.sequence_nr_))
   {
      if (genericSend(fd(), f, 0))
      {
         if (!disp_->isRunning())
            disp_->loopUntil(f.sequence_nr_);
      }
   }
}


inline
int StubBase::connect(bool block)
{
   return disp_->connect(*this, block);
}


void StubBase::sendSignalUnregistration(ClientSignalBase& sigbase)
{
   assert(disp_);
   
   UnregisterSignalFrame f(disp_->removeSignalRegistration(sigbase));
   f.payloadsize_ = 0;
   f.sequence_nr_ = disp_->generateSequenceNr();
   
   genericSend(fd(), f, 0);
}


template<template<template<typename, typename, typename> class, 
                  template<typename, typename, typename> class,
                  template<typename, typename, typename> class> 
   class IfaceT>
struct Stub : StubBase, IfaceT<ClientRequest, ClientResponse, ClientSignal>
{   
   friend struct Dispatcher;
   
private:

   typedef IfaceT<ClientRequest, ClientResponse, ClientSignal> interface_type;
   
public:
   
   Stub(const char* role, const char* boundname)
    : StubBase(InterfaceNamer<interface_type>::name(), role, boundname)
   {
      std::for_each(((interface_type*)this)->container_.begin(), ((interface_type*)this)->container_.end(), std::tr1::bind(&StubBase::reparent, this, _1));
      ((interface_type*)this)->container_.clear();
   }
   
   /// blocking connect to server
   inline
   bool connect()
   {
      bool rc = false;
      
      if (fd_ <= 0)
      {
         int fd = StubBase::connect(true);   // inherited friendship
         
         if (fd > 0)
         {
            fd_ = fd;
            rc = true;
         }
      }
      else 
         rc = true;
      
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
    , sequence_nr_(0)
   {
      // NOOP
   }
   
   inline
   ServerRequestDescriptor(const ServerRequestDescriptor& rhs)
    : requestor_(rhs.requestor_)
    , fd_(rhs.fd_)
    , sequence_nr_(rhs.sequence_nr_)
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
         
         const_cast<ServerRequestDescriptor&>(rhs).clear();
      }
      
      return *this;
   }
   
   ServerRequestDescriptor& set(ServerRequestBase* requestor, int fd, uint32_t sequence_nr)
   {
      requestor_ = requestor;
      fd_ = fd;
      sequence_nr_ = sequence_nr;
      
      return *this;
   }
   
   inline
   void clear()
   {
      set(0, -1, 0);
   }
   
   inline
   operator const void*()
   {
      return requestor_;
   }
   
   ServerRequestBase* requestor_;
   int fd_;
   uint32_t sequence_nr_;
};


template<template<template<typename,typename,typename> class, 
                  template<typename, typename, typename> class,
                  template<typename,typename,typename> class> 
   class IfaceT>
struct Skeleton : IfaceT<ServerRequest, ServerResponse, ServerSignal>
{
   friend struct Dispatcher;
   template<typename SkelT> friend struct ServerHolder;
   
   typedef IfaceT<ServerRequest, ServerResponse, ServerSignal> interface_type;
   
   Skeleton(const char* role)
    : role_(role)
   {
      assert(role_);
   }
   
   /// only valid within request handler
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
   
   /// only valid within request handler - must be called in order to respond to the request later in time
   inline
   ServerRequestDescriptor deferResponse()
   {
      assert(current_request_);
      assert(current_request_.requestor_->hasResponse());  
      
      return current_request_;   // invalidates the current request!
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
   
   
private:
   
   void handleRequest(uint32_t funcid, uint32_t sequence_nr, int fd, void* payload, size_t length)
   {
      //std::cout << "Skeleton::handleRequest '" << funcid << "'" << std::endl;
      std::map<uint32_t, ServerRequestBase*>::iterator iter = ((interface_type*)this)->container_.find(funcid);
      
      if (iter != ((interface_type*)this)->container_.end())
      {
         try
         {
            current_request_.set(iter->second, fd, sequence_nr);
            iter->second->eval(payload, length);
            
            // current_request_ is only valid if no response handler was called
            if (current_request_)
            {
                // in that case the request must not have a reponse
               assert(!current_request_.requestor_->hasResponse());  
               current_request_.clear();
            }
         }
         catch(...)
         {
            current_request_.clear();
            throw;
         }
      }
      else
         std::cerr << "Unknown request '" << funcid << "' with payload size=" << length << std::endl;
   }
   
   
   const char* role_;
   ServerRequestDescriptor current_request_;
};


// interface checker helper
template<typename ServerT>
struct isServer
{
private:
   
   template<template<template<typename,typename,typename> class, 
                     template<typename, typename, typename> class,
                     template<typename,typename,typename> class> 
   class IfaceT>
   static int testFunc(const Skeleton<IfaceT>*);

   static char testFunc(...);   
   
public:
   
   enum { value = (sizeof(testFunc((ServerT*)0)) == sizeof(int) ? true : false) };
};


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
            template<typename, typename=Void, typename=Void> class Response, \
            template<typename=Void,typename=Void,typename=Void> class Signal> \
      struct iface; \
            \
   template<> struct InterfaceNamer<iface<ClientRequest, ClientResponse, ClientSignal> > { static inline const char* const name() { return #  iface ; } }; \
   template<> struct InterfaceNamer<iface<ServerRequest, ServerResponse, ServerSignal> > { static inline const char* const name() { return #  iface ; } }; \
   template<template<typename=Void,typename=Void,typename=Void> class Request, \
            template<typename, typename=Void, typename=Void> class Response, \
            template<typename=Void,typename=Void,typename=Void> class Signal> \
      struct iface : InterfaceBase<Request>

#define INIT_REQUEST(request) \
   request(((AbsoluteInterfaceBase*)this)->nextId(), ((InterfaceBase<Request>*)this)->container_)

#define INIT_RESPONSE(response) \
   response()

#define INIT_SIGNAL(signal) \
   signal(((AbsoluteInterfaceBase*)this)->nextId(), ((InterfaceBase<Request>*)this)->signals_)

#endif   // IPC2_H