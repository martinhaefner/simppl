#ifndef SIMPPL_DETAIL_SERVERHOLDER_H
#define SIMPPL_DETAIL_SERVERHOLDER_H


#include "simppl/detail/serversignalbase.h"


namespace simppl
{
   
namespace ipc
{

namespace detail
{

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

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERVERHOLDER_H