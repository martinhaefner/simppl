#include <iostream>
#include <algorithm>

#include "simppl/dispatcher.h"
#include "simppl/stub.h"
#include "simppl/sbroker.h"
#include "simppl/blocking.h"


namespace spl = simppl::ipc;


inline
void printServiceInfo(const spl::ServiceInfo& info)
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
   spl::Dispatcher disp;

   spl::Stub<::Broker> broker("broker", "unix:the_broker");
   disp.addClient(broker);
   
   try
   {
      broker.connect();
      
      std::vector<spl::ServiceInfo> services;
      broker.listServices() >> services;
      
      std::cout << "Available services: " << std::endl;
      std::for_each(services.begin(), services.end(), printServiceInfo);
      std::cout << std::endl;
      
      std::vector<std::string> waiters;
      broker.listWaiters() >> waiters;
      
      std::cout << "Waiters waiting for: " << std::endl;
      std::for_each(waiters.begin(), waiters.end(), printWaiterInfo);
      std::cout << std::endl;
   }
   catch(const std::exception& ex)
   {
      std::cerr << "Exception occured: " << ex.what() << std::endl;
      return EXIT_FAILURE;
   }
   
   return EXIT_SUCCESS;
}
