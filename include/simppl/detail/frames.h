#ifndef SIMPPL_DETAIL_FRAMES_H
#define SIMPPL_DETAIL_FRAMES_H


#include "simppl/detail/constants.h"

#include <cstdint>
#include <cstring>


namespace simppl
{
   
namespace ipc
{

namespace detail
{

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

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_FRAMES_H