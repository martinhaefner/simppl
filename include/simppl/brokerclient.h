#ifndef SIMPPL_BROKERCLIENT_H
#define SIMPPL_BROKERCLIENT_H


#include <functional>


namespace simppl
{
   
namespace ipc
{

// forward decl   
struct Dispatcher;


struct BrokerClient 
{
   BrokerClient(Dispatcher& disp);   
   ~BrokerClient();
   
   void registerService(const std::string& fullName, const std::string& location);
   
   void waitForService(const std::string& fullName, std::function<void(std::string, std::string)> func);
   
      
private:
   
   BrokerClient(const BrokerClient&) = delete;
   BrokerClient& operator=(const BrokerClient&) = delete;
   
   struct BrokerStub;
   BrokerStub* stub_;
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_BROKERCLIENT_H
