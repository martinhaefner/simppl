#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <thread>


using namespace std::placeholders;

using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{
   
INTERFACE(Errors)
{   
   Request<simppl::dbus::Oneway> stop;
   
   Request<> hello;
   Request<in<int>, out<int>> hello1;
   
   inline
   Errors()
    : INIT(stop)
    , INIT(hello)
    , INIT(hello1)
   {
      // NOOP
   }
};

}

using namespace test;


namespace {
   

struct Client : simppl::dbus::Stub<Errors>
{
   Client()   
    : simppl::dbus::Stub<Errors>("s", "unix:ErrorsTest")    
   {
      connected >> std::bind(&Client::handleConnected, this, _1);
      
      hello >> std::bind(&Client::handleHello, this, _1);
      hello1 >> std::bind(&Client::handleHello1, this, _1, _2);
   }
   
   
   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);
      hello.async();
   }
   
   
   void handleHello(simppl::dbus::CallState state)
   {
      EXPECT_FALSE((bool)state);
      EXPECT_FALSE(state.isTransportError());
      EXPECT_TRUE(state.isRuntimeError());
      EXPECT_EQ(0, strcmp(state.what(), "Shit happens"));
      
      hello1.async(42);
   }
   
   
   void handleHello1(simppl::dbus::CallState state, int)
   {
      EXPECT_FALSE((bool)state);
      EXPECT_FALSE(state.isTransportError());
      EXPECT_TRUE(state.isRuntimeError());
      EXPECT_EQ(0, strcmp(state.what(), "Also shit"));
      
      disp().stop();
   }
};


struct Server : simppl::dbus::Skeleton<Errors>
{
   Server(const char* rolename)
    : simppl::dbus::Skeleton<Errors>(rolename)
   {
      stop >> std::bind(&Server::handleStop, this);
      hello >> std::bind(&Server::handleHello, this);
      hello1 >> std::bind(&Server::handleHello1, this, _1);
   }
   
   void handleStop()
   {
      disp().stop();
   }
   
   void handleHello()
   {
      respondWith(simppl::dbus::RuntimeError(-1, "Shit happens"));
   }
   
   void handleHello1(int i)
   {
      respondWith(simppl::dbus::RuntimeError(-2, "Also shit"));
   }
};


}   // anonymous namespace


TEST(Errors, methods) 
{
   simppl::dbus::Dispatcher d("dbus:session");
   Client c;
   Server s("s");
   
   d.addClient(c);
   d.addServer(s);
   
   d.run();
}


static void blockrunner()
{
   simppl::dbus::Dispatcher d("dbus:session");

   Server s("s");
   d.addServer(s);
   
   d.run();
}


TEST(Errors, blocking) 
{
   std::thread t(blockrunner);
   
   simppl::dbus::Dispatcher d("dbus:session");
      
   simppl::dbus::Stub<Errors> stub("s", "unix:ErrorsTest");
   d.addClient(stub);
   
   stub.connect();
   
   try
   {
      stub.hello();
      
      // never reach
      ASSERT_FALSE(true);
   }
   catch(const simppl::dbus::RuntimeError& e)
   {
      EXPECT_EQ(0, strcmp(e.what(), "Shit happens"));
   }
   catch(...)
   {
      ASSERT_FALSE(true);
   }
   
   try
   {
      int res = stub.hello1(101);
      
      // never reach
      ASSERT_FALSE(true);
   }
   catch(const simppl::dbus::RuntimeError& e)
   {
      EXPECT_EQ(0, strcmp(e.what(), "Also shit"));
   }
   catch(...)
   {
      ASSERT_FALSE(true);
   }
   
   stub.stop();
   t.join();
}
