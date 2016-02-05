#ifndef SIMPPL_DETAIL_SERVERHOLDER_H
#define SIMPPL_DETAIL_SERVERHOLDER_H


namespace simppl
{

namespace ipc
{

namespace detxxail
{

struct ServerHolderBase
{
   virtual ~ServerHolderBase()
   {
      // NOOP
   }

   virtual std::string fqn() const = 0;

   /// handle request
   virtual void eval(uint32_t funcid, uint32_t sequence_nr, uint32_t sessionid, int fd, const void* payload, size_t len) = 0;

   virtual ServerSignalBase* addSignalRecipient(uint32_t id, int fd, uint32_t registrationid, uint32_t clientsid) = 0;
};


// FIXME maybe obsolete
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

   std::string fqn() const
   {
      return handler_->fqn();
   }

   /*virtual*/
   void eval(uint32_t funcid, uint32_t sequence_nr, uint32_t sessionid, int fd, const void* payload, size_t len)
   {
      handler_->handleRequest(funcid, sequence_nr, sessionid, fd, payload, len);
   }

   SkeletonT* handler_;
};

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERVERHOLDER_H
