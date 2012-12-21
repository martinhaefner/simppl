#ifndef SIMPPL_DETAIL_SERVERSIGNALBASE_H
#define SIMPPL_DETAIL_SERVERSIGNALBASE_H


#include "simppl/detail/signalrecipient.h"
#include "simppl/detail/serialization.h"


namespace detail
{

struct ServerSignalBase
{   
   void sendSignal(const Serializer& s);
   
   void addRecipient(int fd, uint32_t registrationid, uint32_t clientsid);

   void removeRecipient(uint32_t registrationid);
   
   virtual void onAttach(uint32_t /*registrationid*/);
   
   /// remove all recipients which may emit to the file descriptor 'fd'
   void removeAllWithFd(int fd);
   
protected:
   
   ~ServerSignalBase();
   
   std::map<uint32_t/*registrationid*/, SignalRecipient> recipients_;
};

}   // namespace detail


#endif   // SIMPPL_DETAIL_SERVERSIGNALBASE_H