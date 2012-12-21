#ifndef SIMPPL_DETAIL_UTIL_H
#define SIMPPL_DETAIL_UTIL_H


#include <sys/uio.h>
#include <sys/socket.h>
#include <cstdio>


namespace detail
{
   
// FIXME move socket operation into .cpp file
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

}   // namespace detail


#endif   // SIMPPL_DETAIL_UTIL_H
