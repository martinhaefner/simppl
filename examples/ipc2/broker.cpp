#include "simppl/ipc2.h"

/*
 * build the four types of servers:
 *
 *   g++ -I../../ -DBROKER -o broker broker.cpp
 *   g++ -I../../ -DSERVER -o server broker.cpp
 *   g++ -I../../ -DBLOCKCLIENT -o blocking_client broker.cpp
 *   g++ -I../../ -o client broker.cpp
 * 
 * Make sure the broker is the first server to run!
 */


INTERFACE(Broker)
{   
   Request<std::string/*servicename*/> waitForService;
   Request<std::string/*servicename*/, std::string/*location*/> registerService;
   
   Response<std::string/*servicename*/, std::string/*location*/> serviceReady;
   
   inline
   Broker()
    : INIT_REQUEST(waitForService)
    , INIT_REQUEST(registerService)
    , INIT_RESPONSE(serviceReady)
   {
      waitForService >> serviceReady;
   }
};


INTERFACE(HelloWorld)
{   
   Request<std::string> sayHello;
     
   inline
   HelloWorld()
    : INIT_REQUEST(sayHello)
   {
      // NOOP
   }
};


// -----------------------------------------------------------------------------------------------------


struct BrokerImpl : Skeleton<Broker>
{
   typedef std::map<std::string, std::string> servicemap_type;
   typedef std::multimap<std::string, ServerRequestDescriptor> waitersmap_type;
   
   BrokerImpl()
    : Skeleton<Broker>("broker")
   {
      waitForService >> std::bind(&BrokerImpl::handleWaitForService, this, _1);
      registerService >> std::bind(&BrokerImpl::handleRegisterService, this, _1, _2);
   }
   
   void handleRegisterService(const std::string& serv, const std::string& loc)
   {
      std::cout << "Registering " << serv << " at location " << loc << std::endl;
      
      services_[serv] = loc;
    
      std::pair<waitersmap_type::iterator, waitersmap_type::iterator> p = waiters_.equal_range(serv);
      
      for(waitersmap_type::iterator iter = p.first; iter != p.second; ++iter)
      {
         respondOn(iter->second, serviceReady(serv, loc));
      }
   }
   
   void handleWaitForService(const std::string& serv)
   {
      std::cout << "Client waits for " << serv << std::endl;
      
      servicemap_type::iterator iter = services_.find(serv);
      if (iter != services_.end())
      {
         respondWith(serviceReady(serv, iter->second));
      }
      else
         waiters_.insert(std::pair<std::string, ServerRequestDescriptor>(serv, deferResponse()));
      
   }
   
   servicemap_type services_;
   waitersmap_type waiters_;
};


struct HelloWorldImpl : Skeleton<HelloWorld>
{
   HelloWorldImpl()
    : Skeleton<HelloWorld>("helloworld")
   {
      sayHello >> std::bind(&HelloWorldImpl::handleSayHello, this, _1);
   }
   
   void handleSayHello(const std::string& str)
   {
      std::cout << "Client says '" << str << "'" << std::endl;
   }
};


// -----------------------------------------------------------------------------------------------------


struct HelloWorldClient : Stub<HelloWorld>
{
   HelloWorldClient(const char* loc)
    : Stub<HelloWorld>("helloworld", loc)
   {
      connected >> std::bind(&HelloWorldClient::handleConnected, this, _1);
   }
   
   void handleConnected(ConnectionState)
   {
      sayHello("Hello World!");
   }
};


struct BrokerClientOnServerSide : Stub<Broker>
{
   BrokerClientOnServerSide()
    : Stub<Broker>("broker", "unix:the_broker")
   {
      connected >> std::bind(&BrokerClientOnServerSide::handleConnected, this, _1);
   }
   
   void handleConnected(ConnectionState)
   {
      std::cout << "Got connected to broker, registering..." << std::endl;
      registerService("HelloWorld::helloworld", "unix:the_server");
   }
};


struct BrokerClientOnClientSide : Stub<Broker>
{
   BrokerClientOnClientSide()
    : Stub<Broker>("broker", "unix:the_broker")
   {
      serviceReady >> std::bind(&BrokerClientOnClientSide::handleServiceReady, this, _1, _2);
      connected >> std::bind(&BrokerClientOnClientSide::handleConnected, this, _1);
   }
   
   void handleConnected(ConnectionState)
   {
      std::cout << "Got connected to broker, waiting..." << std::endl;
      
      waitForService("HelloWorld::helloworld");
   }
   
   void handleServiceReady(const std::string& service, const std::string& loc)
   {
      std::cout << "Got service ready signal from broker, connecting to '" << service << "' at location '" << loc << "'" << std::endl;
      
      HelloWorldClient* impl = new HelloWorldClient(loc.c_str());
      disp().addClient(*impl);
   }
};


// -----------------------------------------------------------------------------------------------------


#ifdef BROKER

int main()
{
   Dispatcher d("unix:the_broker");
   
   BrokerImpl brk;
   d.addServer(brk);

   return d.run();
}

#elif defined(SERVER)

int main()
{
   Dispatcher d("unix:the_server");

   HelloWorldImpl hwi;
   d.addServer(hwi);

   BrokerClientOnServerSide brk;
   d.addClient(brk);
   
   return d.run();
}

#elif defined(BLOCKCLIENT)

int main()
{
   Dispatcher d;
   
   Stub<Broker> brk("broker", "unix:the_broker");
   d.addClient(brk);
   
   if (brk.connect())
   {
      std::string loc;
      d.waitForResponse(brk.waitForService("HelloWorld::helloworld"), loc);
      
      if (!loc.empty())
      {
         HelloWorldClient impl(loc.c_str());
         d.addClient(impl);
         
         if (impl.connect())
         {
            impl.sayHello("Hello blocking World!");
         }
      }
   }
   
   return EXIT_SUCCESS;
}

#else   // CLIENT

int main()
{
   Dispatcher d;

   BrokerClientOnClientSide brk;
   d.addClient(brk);

   return d.run();
}

#endif
