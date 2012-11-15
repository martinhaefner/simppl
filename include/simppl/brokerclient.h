#ifndef SIMPPL_BROKERCLIENT_H
#define SIMPPL_BROKERCLIENT_H


#include "simppl/sbroker.h"

#include <map>


struct BrokerClient 
{
   typedef std::multimap<std::string, std::function<void(std::string, std::string)> > waitersmap_type;
   
   inline
   BrokerClient(Dispatcher& disp)
    : stub_()
   {
      disp.addClient(stub_);
   }
   
   
   void registerService(const std::string& fullName, const std::string& location)
   {
      if (stub_.isConnected())
      {
         stub_.registerService(fullName, location);
      }
      else
      {
         stub_.cache_[fullName] = location;
      }
   }
   
   
   template<typename FuncT>
   void waitForService(const std::string& fullName, FuncT func)
   {
      if (stub_.isConnected())
      {
         stub_.waiters_.insert(waitersmap_type::value_type(fullName, func));
         stub_.waitForService(fullName);
      }
      else
      {
         stub_.waitersCache_.insert(waitersmap_type::value_type(fullName, func));
      }
   }
   
      
private:
 
   // forward decl
   struct BrokerStub;
   
   
   struct Register
   {
      inline
      Register(Stub<Broker>& stub)
       : stub_(stub)
      {
         // NOOP
      }
      
      inline
      void operator()(const std::pair<std::string, std::string>& p) const
      {
         stub_.registerService(p.first, p.second);
      }
      
      Stub<Broker>& stub_;
   };

   
   struct WaitFor
   {
      inline
      WaitFor(BrokerStub& stub)
       : stub_(stub)
      {
         // NOOP
      }
      
      inline
      void operator()(const waitersmap_type::value_type& p) const
      {
         stub_.waitForService(p.first);
         stub_.waiters_.insert(waitersmap_type::value_type(p.first, p.second));
      }
      
      BrokerStub& stub_;
   };
   
   
   struct BrokerStub : Stub<Broker>
   {
      
      inline
      BrokerStub()
       : Stub<Broker>("broker", "unix:the_broker")
      {
         serviceReady >> std::bind(&BrokerStub::handleServiceReady, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
         connected >> std::bind(&BrokerStub::handleConnected, this);
      }
      
      void handleConnected()
      {
         if (!cache_.empty())
         {
            std::for_each(cache_.begin(), cache_.end(), Register(*this));
            cache_.clear();
         }
         
         if (!waitersCache_.empty())
         {
            std::for_each(waitersCache_.begin(), waitersCache_.end(), WaitFor(*this));
            waitersCache_.clear();
         }
      }
      
      void handleServiceReady(const CallState& state, const std::string& fullName, const std::string& location)
      {
         if (state)
         {
            std::pair<waitersmap_type::iterator, waitersmap_type::iterator> p = waiters_.equal_range(fullName);
         
            for(waitersmap_type::iterator iter = p.first; iter != p.second; ++iter)
            {
               iter->second(fullName, location);
            }
         }
         // else FIXME
      }
      
      std::map<std::string, std::string> cache_;
      
      waitersmap_type waiters_;
      waitersmap_type waitersCache_;
   };

   BrokerStub stub_;
};


#endif   // SIMPPL_BROKERCLIENT_H
