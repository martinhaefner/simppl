#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"
#include "simppl/blocking.h"

#include <thread>


using namespace std::placeholders;


namespace test
{

INTERFACE(Errors)
{
   Request<> hello;
   Request<int> hello1;

   Response<> world;
   Response<int> world1;

   inline
   Errors()
    : INIT(hello)
    , INIT(hello1)
    , INIT(world)
    , INIT(world1)
   {
      hello >> world;
      hello1 >> world1;
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

      world >> std::bind(&Client::handleWorld, this, _1);
      world1 >> std::bind(&Client::handleWorld1, this, _1, _2);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);
      hello();
   }


   void handleWorld(simppl::dbus::CallState state)
   {
      EXPECT_FALSE((bool)state);
      EXPECT_FALSE(state.isTransportError());
      EXPECT_TRUE(state.isRuntimeError());
      EXPECT_EQ(0, strcmp(state.what(), "Shit happens"));

      hello1(42);
   }


   void handleWorld1(simppl::dbus::CallState state, int)
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
      hello >> std::bind(&Server::handleHello, this);
      hello1 >> std::bind(&Server::handleHello1, this, _1);
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
   simppl::dbus::Dispatcher d("bus:session");
   Client c;
   Server s("s");

   d.addClient(c);
   d.addServer(s);

   d.run();
}


TEST(Errors, blocking)
{
   simppl::dbus::Dispatcher d("bus:session");

   Server s("s");
   d.addServer(s);

   simppl::dbus::Stub<Errors> stub("s", "unix:ErrorsTest");
   d.addClient(stub);

   stub.connect();

   try
   {
      stub.hello() >> std::nullptr_t();

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
      int res;
      stub.hello1(101) >> res;

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

   try
   {
      int res;

      // forget handler
      stub.hello1(101);
      stub.hello1(101) >> res;

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
}
