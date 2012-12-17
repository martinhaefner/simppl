#include "simppl/dispatcher.h"
#include "simppl/sbroker.h"
#include "simppl/skeleton.h"


using namespace std::placeholders;


struct ServiceHandle
{
   ServiceHandle()
    : registrationfd_(0)
   {
      // NOOP
   }
   
   ServiceHandle(const std::string& location, int fd)
    : registrationfd_(fd)
    , location_(location)
   {
      // NOOP
   }
   
   int registrationfd_;
   std::string location_;
};


struct PushBackService
{
   inline
   PushBackService(std::vector<ServiceInfo>& v)
    : v_(v)
   {
      // NOOP
   }
   
   inline
   void operator()(const std::pair<std::string, ServiceHandle>& entry) const
   {
      v_.push_back(ServiceInfo(entry.first, entry.second.location_));
   }
   
   std::vector<ServiceInfo>& v_;
};


struct PushBackWaiter
{
   inline
   PushBackWaiter(std::vector<std::string>& v)
    : v_(v)
   {
      // NOOP
   }
   
   template<typename T>
   inline
   void operator()(const T& entry) const
   {
      v_.push_back(entry.first);
   }
   
   std::vector<std::string>& v_;
};


struct BrokerImpl : Skeleton<Broker>
{
   typedef std::map<std::string, ServiceHandle> servicemap_type;
   typedef std::multimap<std::string, ServerRequestDescriptor> waitersmap_type;
   
   inline
   BrokerImpl()
    : Skeleton<Broker>("broker")
   {
      waitForService >> std::bind(&BrokerImpl::handleWaitForService, this, _1);
      registerService >> std::bind(&BrokerImpl::handleRegisterService, this, _1, _2);
      listServices >> std::bind(&BrokerImpl::handleListServices, this);
      listWaiters >> std::bind(&BrokerImpl::handleListWaiters, this);
   }
   
   void handleListServices()
   {
      std::vector<ServiceInfo> rc;
      std::for_each(services_.begin(), services_.end(), PushBackService(rc));
      
      respondWith(serviceList(rc));
   }
   
   void handleListWaiters()
   {
      std::vector<std::string> rc;
      std::for_each(waiters_.begin(), waiters_.end(), PushBackWaiter(rc));
      
      respondWith(waitersList(rc));
   }
   
   void handleRegisterService(const std::string& serv, const std::string& loc)
   {
      std::cout << "Registering " << serv << " at location " << loc << std::endl;
      
      services_[serv] = ServiceHandle(loc, currentRequest().fd_);
    
      std::pair<waitersmap_type::iterator, waitersmap_type::iterator> p = waiters_.equal_range(serv);
      
      for(waitersmap_type::iterator iter = p.first; iter != p.second; ++iter)
      {
         respondOn(iter->second, serviceReady(serv, loc));
      }
      
      waiters_.erase(p.first, p.second);
   }
   
   void handleWaitForService(const std::string& serv)
   {
      std::cout << "Client waits for " << serv << std::endl;
      
      servicemap_type::iterator iter = services_.find(serv);
      if (iter != services_.end())
      {
         respondWith(serviceReady(serv, iter->second.location_));
      }
      else
         waiters_.insert(std::pair<std::string, ServerRequestDescriptor>(serv, deferResponse()));
   }
   
   void removeAllFrom(int fd)
   {
      for (servicemap_type::iterator iter = services_.begin(); iter != services_.end(); )
      {
         if (iter->second.registrationfd_ == fd)
         {
            services_.erase(iter);
            iter = services_.begin();   // not the very best algorithm
         }
         else
            ++iter;
      }
   
      for (waitersmap_type::iterator iter = waiters_.begin(); iter != waiters_.end(); )
      {
         if (iter->second.fd_ == fd)
         {
            waiters_.erase(iter);
            iter = waiters_.begin();   // not the very best algorithm
         }
         else
            ++iter;
      }
   }
   
private:
   
   servicemap_type services_;
   waitersmap_type waiters_;
};


struct BrokerDispatcher : Dispatcher
{
   BrokerDispatcher(BrokerImpl& impl)
    : Dispatcher("unix:the_broker")
    , impl_(impl)
   {
      addServer(impl);
   }
   
   void socketDisconnected(int fd)
   {
      impl_.removeAllFrom(fd);
   }

   BrokerImpl& impl_;
};


int main()
{
   BrokerImpl impl;
   BrokerDispatcher disp(impl);
   
   return disp.run();
}
