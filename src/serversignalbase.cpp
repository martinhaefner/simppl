#include "simppl/detail/serversignalbase.h"


void ServerSignalBase::sendSignal(const Serializer& s)
{
   std::for_each(recipients_.begin(), recipients_.end(), SignalSender(s.data(), s.size()));
}


void ServerSignalBase::addRecipient(int fd, uint32_t registrationid, uint32_t clientsid)
{
   SignalRecipient rcpt(fd, clientsid);
   recipients_[registrationid] = rcpt;
}


void ServerSignalBase::removeRecipient(uint32_t registrationid)
{
   recipients_.erase(registrationid);
}


void ServerSignalBase::onAttach(uint32_t /*registrationid*/)
{
   // NOOP
}


void ServerSignalBase::removeAllWithFd(int fd)
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


ServerSignalBase::~ServerSignalBase()
{
   // NOOP
}
