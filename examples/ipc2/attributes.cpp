#include "simppl/ipc2.h"


struct MyStruct
{
   typedef make_serializer<int, std::string>::type serializer_type;
   
   inline
   MyStruct()
    : i(-1)
   {
      // NOOP
   }
   
   inline
   MyStruct(int _i, const std::string& _str)
    : i(_i)
    , str(_str)
   {
      // NOOP
   }
   
   inline
   bool operator!=(const MyStruct& rhs) const
   {
      return i != rhs.i;
   }
   
   int i;
   std::string str;
};


INTERFACE(Interface)
{   
   Request<int> doSomething;
   Response<int> resultOfDoSomething;

   Attribute<int> myInt;
   Attribute<MyStruct> myStruct;
   Attribute<std::vector<int> > myVector;
   Attribute<int, Committed> comInt;
   
   inline
   Interface()
    : INIT_REQUEST(doSomething)
    , INIT_RESPONSE(resultOfDoSomething)
    , INIT_ATTRIBUTE(myInt)
    , INIT_ATTRIBUTE(myStruct)
    , INIT_ATTRIBUTE(myVector)
    , INIT_ATTRIBUTE(comInt)
   {
      doSomething >> resultOfDoSomething;
   }
};


struct Server : Skeleton<Interface>
{
   Server(const char* role)
    : Skeleton<Interface>(role)
   {      
      myStruct = MyStruct(0, "No answer yet.");
      comInt = 41;
      
      doSomething >> std::bind(&Server::handleDoSomething, this, _1);
   }
   
   void handleDoSomething(int i)
   {
      std::cout << "handleDoSomething with i=" << i << std::endl;
      
      comInt = 42;
      comInt = 43;
      comInt = 44;
      comInt.commit();
      comInt = 45;
      
      // these assignments raise signals if there is anybody connected to them
      myInt = i;
      myVector.push_back(i);
      myStruct = MyStruct(i, "The answer to all questions.");
      
      respondWith(resultOfDoSomething(i));
   }
};


struct Client : Stub<Interface>
{
   Client(const char* role)
    : Stub<Interface>(role, "unix:myserver")   // connect the client to 'myserver'
   {
      connected >> std::bind(&Client::handleConnected, this);
      
      std::cout << "Initial attribute myStruct is [" << myStruct.value().i << ", '" << myStruct.value().str << "']" << std::endl;
      
      resultOfDoSomething >> std::bind(&Client::handleResultDoSomething, this, _1, _2);
   }
   
   void handleConnected()
   {
      myInt.attach() >> std::bind(&Client::attributeChanged, this, _1);
      myStruct.attach();   // here receive status updates, but no direct change notification
      myVector.attach() >> std::bind(&Client::vectorChanged, this, _1, _2, _3, _4);
      
      comInt.attach() >> std::bind(&Client::commAttributeChanged, this, _1);
      doSomething(42);
   }
   
   void handleResultDoSomething(const CallState& state, int response)
   {
      std::cout << "Response is " << response << std::endl;
      std::cout << "Attribute myStruct is now [" << myStruct.value().i << ", '" << myStruct.value().str << "']" << std::endl;
      std::cout << "Finishing..." << std::endl;
      disp().stop();
   }
   
   void attributeChanged(int i)
   {
      std::cout << "Attribute myStruct is initialized from server side [" << myStruct.value().i << ", '" << myStruct.value().str << "']" << std::endl;
      std::cout << "attribute myInt changed: new value=" << i << ", or new value=" << myInt.value() << " respectively." << std::endl;
   }
   
   void commAttributeChanged(int i)
   {
      std::cout << "Attribute was committed: value=" << i << std::endl;
   }

   void vectorChanged(const std::vector<int>& iv, How how, uint32_t where, uint32_t len)
   {
      std::cout << "Yes, vector changed(" << how << "," << where << "," << len << "): ";

      if (iv.empty())
      {
         std::cout << "[empty]";
      }
      else
         std::cout << iv.front();

      std::cout << std::endl;
   }
};


void* server(void* dispatcher)
{
   Dispatcher& d = *(Dispatcher*)dispatcher;
   
   Server s1("myrole");
   d.addServer(s1);
   
   d.run();
   return 0;
}


int main()
{
   // start server dispatcher thread on unix path 'myserver'
   Dispatcher server_dispatcher("unix:myserver");
   
   pthread_t tid;
   pthread_create(&tid, 0, server, &server_dispatcher);
   while(!server_dispatcher.isRunning());   // wait for other thread
   
   // run client in separate thread (not really necessary, just for blocking interfaces)
   Dispatcher d;
   
   Client c("myrole");
   d.addClient(c);
   
   d.run();
   
   server_dispatcher.stop();
   pthread_join(tid, 0);
   
   return 0;

}
