#include "simppl/brokerclient.h"

#include "simppl/stub.h"
#include "simppl/sbroker.h"

#include "simppl/dispatcher.h"


namespace simppl
{
   
namespace ipc
{


struct BrokerClient::BrokerStub : Stub< ::Broker>
{      
   inline
   BrokerStub()
    : Stub< ::Broker>("broker", "unix:the_broker")
   {
      serviceReady >> std::bind(&BrokerStub::handleServiceReady, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
      connected >> std::bind(&BrokerStub::handleConnected, this, std::placeholders::_1);
   }
   
   
   void handleConnected(simppl::ipc::ConnectionState s)
   {
      if (s == simppl::ipc::ConnectionState::Connected)
      {
         if (!cache_.empty())
         {
            std::for_each(cache_.begin(), cache_.end(), [this](decltype(*cache_.begin())& p){
               this->registerService(p.first, p.second);
            });
            
            cache_.clear();
         }
         
         if (!waitersCache_.empty())
         {
            std::for_each(waitersCache_.begin(), waitersCache_.end(), [this](decltype(*waitersCache_.begin())& p){
               this->waitForService(p.first);
               this->waiters_.insert(std::make_pair(p.first, p.second));
            });
            
            waitersCache_.clear();
         }
      }
   }
   
   
   void handleServiceReady(const CallState& state, const std::string& fullName, const std::string& location)
   {
      if (state)
      {
         auto p = waiters_.equal_range(fullName);
      
         for(auto iter = p.first; iter != p.second; ++iter)
         {
            iter->second(fullName, location);
         }
      }
      // else FIXME
   }
   
   std::map<std::string, std::string> cache_;
   
   typedef std::multimap<std::string, std::function<void(std::string, std::string)> > waitersmap_type;
   
   waitersmap_type waiters_;
   waitersmap_type waitersCache_;
};


// ---------------------------------------------------------------------


BrokerClient::BrokerClient(Dispatcher& disp)
 : stub_(new BrokerStub())
{
   disp.addClient(*stub_);
}


BrokerClient::~BrokerClient()
{
   delete stub_;
}
   
   
void BrokerClient::registerService(const std::string& fullName, const std::string& location)
{
   if (stub_->isConnected())
   {
      stub_->registerService(fullName, location);
   }
   else
   {
      stub_->cache_[fullName] = location;
   }
}


void BrokerClient::waitForService(const std::string& fullName, std::function<void(std::string, std::string)> func)
{
   if (stub_->isConnected())
   {
      stub_->waiters_.insert(std::make_pair(fullName, func));
      stub_->waitForService(fullName);
   }
   else
      stub_->waitersCache_.insert(std::make_pair(fullName, func));
}

         
}   // namespace ipc

}   // namespace simppl
