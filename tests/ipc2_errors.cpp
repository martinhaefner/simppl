#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <thread>


using namespace std::placeholders;


INTERFACE(Errors)
{   
   Request<> hello;
   Request<int> hello1;
   
   Response<> world;
   Response<int> world1;
   
   inline
   Errors()
    : INIT_REQUEST(hello)
    , INIT_REQUEST(hello1)
    , INIT_RESPONSE(world)
    , INIT_RESPONSE(world1)
   {
      hello >> world;
      hello1 >> world1;
   }
};


namespace {
   

struct Client : simppl::ipc::Stub<Errors>
{
   Client()   
    : simppl::ipc::Stub<Errors>("s", "unix:ErrorsTest")    
   {
      connected >> std::bind(&Client::handleConnected, this, _1);
      
      world >> std::bind(&Client::handleWorld, this, _1);
      world1 >> std::bind(&Client::handleWorld1, this, _1, _2);
   }
   
   
   void handleConnected(simppl::ipc::ConnectionState s)
   {
      EXPECT_EQ(simppl::ipc::ConnectionState::Connected, s);
      hello();
   }
   
   
   void handleWorld(const simppl::ipc::CallState& state)
   {
      EXPECT_FALSE((bool)state);
      EXPECT_FALSE(state.isTransportError());
      EXPECT_TRUE(state.isRuntimeError());
      EXPECT_EQ(0, strcmp(state.what(), "Shit happens"));
      
      hello1(42);
   }
   
   
   void handleWorld1(const simppl::ipc::CallState& state, int)
   {
      EXPECT_FALSE((bool)state);
      EXPECT_FALSE(state.isTransportError());
      EXPECT_TRUE(state.isRuntimeError());
      EXPECT_EQ(0, strcmp(state.what(), "Also shit"));
      
      disp().stop();
   }
};


struct Server : simppl::ipc::Skeleton<Errors>
{
   Server(const char* rolename)
    : simppl::ipc::Skeleton<Errors>(rolename)
   {
      hello >> std::bind(&Server::handleHello, this);
      hello1 >> std::bind(&Server::handleHello1, this, _1);
   }
   
   void handleHello()
   {
      respondWith(simppl::ipc::RuntimeError(-1, "Shit happens"));
   }
   
   void handleHello1(int i)
   {
      respondWith(simppl::ipc::RuntimeError(-2, "Also shit"));
   }
};


}   // anonymous namespace


TEST(Errors, methods) 
{
   simppl::ipc::Dispatcher d("unix:ErrorsTest");
   Client c;
   Server s("s");
   
   d.addClient(c);
   d.addServer(s);
   
   d.run();
}


TEST(Errors, blocking) 
{
   simppl::ipc::Dispatcher d("unix:ErrorsTest");
   
   Server s("s");
   d.addServer(s);
   
   simppl::ipc::Stub<Errors> stub("s", "unix:ErrorsTest");
   d.addClient(stub);
   
   stub.connect();
   
   try
   {
      d.waitForResponse(stub.hello());
      
      // never reach
      ASSERT_FALSE(true);
   }
   catch(const simppl::ipc::RuntimeError& e)
   {
      EXPECT_EQ(0, strcmp(e.what(), "Shit happens"));
   }
   catch(...)
   {
      ASSERT_FALSE(true);
   }
   
   try
   {
      int res;
      stub.hello1(101) >> res;
      
      // never reach
      ASSERT_FALSE(true);
   }
   catch(const simppl::ipc::RuntimeError& e)
   {
      EXPECT_EQ(0, strcmp(e.what(), "Also shit"));
   }
   catch(...)
   {
      ASSERT_FALSE(true);
   }
}
