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
   Request<simppl::dbus::oneway> stop;

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
   Client(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Errors>(d, "s")
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
      EXPECT_STREQ(state.exception().name(), "shit.happens");

      hello1.async(42);
   }


   void handleHello1(simppl::dbus::CallState state, int)
   {
      EXPECT_FALSE((bool)state);
      EXPECT_STREQ(state.exception().name(), "also.shit");

      disp().stop();
   }
};


struct Server : simppl::dbus::Skeleton<Errors>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Errors>(d, rolename)
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
      respondWith(simppl::dbus::Error("shit.happens"));
   }

   void handleHello1(int i)
   {
      respondWith(simppl::dbus::Error("also.shit"));
   }
};


}   // anonymous namespace


TEST(Errors, methods)
{
   simppl::dbus::Dispatcher d("bus:session");
   Client c(d);
   Server s(d, "s");

   d.run();
}


static void blockrunner()
{
   simppl::dbus::Dispatcher d("bus:session");
   Server s(d, "s");

   d.run();
}


TEST(Errors, blocking)
{
   std::thread t(blockrunner);

   simppl::dbus::Dispatcher d("bus:session");

   simppl::dbus::Stub<Errors> stub(d, "s");

   stub.connect();

   try
   {
      stub.hello();

      // never reach
      ASSERT_FALSE(true);
   }
   catch(const simppl::dbus::Error& e)
   {
      EXPECT_STREQ(e.name(), "shit.happens");
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
   catch(const simppl::dbus::Error& e)
   {
      EXPECT_STREQ(e.name(), "also.shit");
   }
   catch(...)
   {
      ASSERT_FALSE(true);
   }

   stub.stop();
   t.join();
}
