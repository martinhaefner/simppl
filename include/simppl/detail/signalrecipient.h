#ifndef SIMPPL_DETAIL_SIGNALRECIPIENT_H
#define SIMPPL_DETAIL_SIGNALRECIPIENT_H

#include <unistd.h>
#include <cstdint>
#include <map>


namespace simppl
{
   
namespace ipc
{

namespace detail
{

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
   
   void operator()(const SignalRecipient& info);
   
   const void* data_;
   size_t len_;
};

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SIGNALRECIPIENT_H
