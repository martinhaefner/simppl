#include "simppl/dispatcher.h"
#include "simppl/sbroker.h"
#include "simppl/skeleton.h"


using namespace std::placeholders;

namespace spl = simppl::ipc;


struct BrokerImpl : spl::Skeleton<::Broker>
{
   BrokerImpl()
    : spl::Skeleton<::Broker>("broker")
   {
      waitForService >> std::bind(&BrokerImpl::handleWaitForService, this, _1);
      registerService >> std::bind(&BrokerImpl::handleRegisterService, this, _1, _2);
      listServices >> std::bind(&BrokerImpl::handleListServices, this);
      listWaiters >> std::bind(&BrokerImpl::handleListWaiters, this);
   }
   
   void handleListServices()
   {
      std::vector<spl::ServiceInfo> rc;
      
      std::for_each(services_.begin(), services_.end(), [&rc](decltype(*services_.begin())& entry){
         rc.push_back(spl::ServiceInfo(entry.first, std::get<0>(entry.second)));
      });
      
      respondWith(serviceList(rc));
   }
   
   void handleListWaiters()
   {
      std::vector<std::string> rc;
      std::for_each(waiters_.begin(), waiters_.end(), [&rc](decltype(*waiters_.begin())& entry){
         rc.push_back(entry.first);
      });
      
      respondWith(waitersList(rc));
   }
   
   void handleRegisterService(const std::string& serv, const std::string& loc)
   {
      std::cout << "Registering " << serv << " at location " << loc << std::endl;
      
      services_[serv] = std::make_tuple(loc, currentRequest().fd_);
    
      auto p = waiters_.equal_range(serv);
      
      for(auto iter = p.first; iter != p.second; ++iter)
      {
         respondOn(iter->second, serviceReady(serv, loc));
      }
      
      waiters_.erase(p.first, p.second);
   }
   
   void handleWaitForService(const std::string& serv)
   {
      std::cout << "Client waits for " << serv << std::endl;
      
      auto iter = services_.find(serv);
      if (iter != services_.end())
      {
         respondWith(serviceReady(serv, std::get<0>(iter->second)));
      }
      else
         waiters_.insert(make_pair(serv, deferResponse()));
   }
   
   void removeAllFrom(int fd)
   {
      for (auto iter = services_.begin(); iter != services_.end(); )
      {
         if (std::get<1>(iter->second) == fd)
         {
            iter = services_.erase(iter);
         }
         else
            ++iter;
      }
   
      for (auto iter = waiters_.begin(); iter != waiters_.end(); )
      {
         if (iter->second.fd_ == fd)
         {
            iter = waiters_.erase(iter);
         }
         else
            ++iter;
      }
   }
   
private:
   
   std::map<std::string, std::tuple<std::string/*location*/, int/*fd*/>> services_;
   std::multimap<std::string, spl::ServerRequestDescriptor> waiters_;
};


void socketStateChange(BrokerImpl& impl, int fd, bool connected)
{
   if (!connected)
      impl.removeAllFrom(fd);
} 


int main()
{
   spl::Dispatcher disp("unix:the_broker");
   
   BrokerImpl impl;
   disp.addServer(impl);
   
   disp.socketStateChanged = std::bind(&socketStateChange, std::ref(impl), _1, _2);
   
   return disp.run();
}
