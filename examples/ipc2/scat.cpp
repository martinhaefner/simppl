#include <iostream>
#include <algorithm>

#include "sbroker.h"


inline
void printServiceInfo(const ServiceInfo& info)
{
   std::cout << "   " << info.name_ << " at " << info.location_ << std::endl;
}


inline
void printWaiterInfo(const std::string& info)
{
   std::cout << "   " << info << std::endl;
}


int main()
{
   Dispatcher disp;

   Stub<Broker> broker("broker", "unix:the_broker");
   disp.addClient(broker);
   
   if (broker.connect())
   {
      std::vector<ServiceInfo> services;
      if (disp.waitForResponse(broker.listServices(), services))
      {
         std::cout << "Available services: " << std::endl;
         std::for_each(services.begin(), services.end(), printServiceInfo);
         std::cout << std::endl;
         
         std::vector<std::string> waiters;
         if (disp.waitForResponse(broker.listWaiters(), waiters))
         {
            std::cout << "Waiters waiting for: " << std::endl;
            std::for_each(waiters.begin(), waiters.end(), printWaiterInfo);
            std::cout << std::endl;
            
            return EXIT_SUCCESS;
         }
      }
   }
   
   return EXIT_FAILURE;
}
