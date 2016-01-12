#include "simppl/detail/serversignalbase.h"

#include "simppl/interface.h"

#define SIMPPL_SERVERSIGNALBASE_CPP
#include "simppl/serverside.h"
#undef SIMPPL_SERVERSIGNALBASE_CPP


namespace simppl
{

namespace ipc
{


// FIXME move to somewhere different 
ServerRequestBase::ServerRequestBase(const char* name, detail::BasicInterface* iface)
 : hasResponse_(false)
 , name_(name)
{
   std::cout << "method register: " << name_ << std::endl;
   dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->methods_[name_] = this;
}


ServerAttributeBase::ServerAttributeBase(const char* name, detail::BasicInterface* iface)
 : name_(name)
{
   std::cout << "attribute register: " << name_ << std::endl;
   dynamic_cast<InterfaceBase<ServerRequest>*>(iface)->attributes_[name_] = this;
}


namespace detail
{
   
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

}   // namespace detail

}   // namespace ipc

}   // namespace simppl
