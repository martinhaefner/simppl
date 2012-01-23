#include "include/ipc2.h"


struct MyStruct
{
   typedef Tuple<int, std::string> type;
   
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
   
   inline
   Interface()
    : INIT_REQUEST(doSomething)
    , INIT_RESPONSE(resultOfDoSomething)
    , INIT_ATTRIBUTE(myInt)
    , INIT_ATTRIBUTE(myStruct)
    , INIT_ATTRIBUTE(myVector)
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
      doSomething >> std::tr1::bind(&Server::handleDoSomething, this, _1);
   }
   
   void handleDoSomething(int i)
   {
      std::cout << "handleDoSomething with i=" << i << std::endl;
      
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
      std::cout << "Initial attribute myStruct is [" << myStruct.value().i << ", '" << myStruct.value().str << "']" << std::endl;
      
      resultOfDoSomething >> std::tr1::bind(&Client::handleResultDoSomething, this, _1);
   }
   
   void connected()
   {
      myInt.attach() >> std::tr1::bind(&Client::attributeChanged, this, _1);
      myStruct.attach();   // here receive status updates, but no direct change notification
      myVector.attach() >> std::tr1::bind(&Client::vectorChanged, this, _1);
      
      doSomething(42);
   }
   
   void handleResultDoSomething(int response)
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

   void vectorChanged(const std::vector<int>& iv)
   {
      std::cout << "Yes, vector changed: ";

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
