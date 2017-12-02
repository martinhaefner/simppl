#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/interface.h"

#include <thread>


using namespace std::literals::chrono_literals;

using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{

INTERFACE(Errors)
{
   Method<simppl::dbus::oneway> stop;

   Method<> hello;
   Method<in<int>, out<int>> hello1;

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
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);
      
         hello.async() >> [this](simppl::dbus::CallState state){
            EXPECT_FALSE((bool)state);
            EXPECT_STREQ(state.exception().name(), "shit.happens");

            hello1.async(42) >> [this](simppl::dbus::CallState state, int){
               EXPECT_FALSE((bool)state);
               EXPECT_STREQ(state.exception().name(), "also.shit");

               disp().stop();
            };
         };
      };
   }
};


struct Server : simppl::dbus::Skeleton<Errors>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Errors>(d, rolename)
   {
      stop >> [this](){ 
         disp().stop(); 
      };
      
      hello >> [this](){
         respond_with(simppl::dbus::Error("shit.happens"));
      };
      
      hello1 >> [this](int){
         respond_with(simppl::dbus::Error("also.shit"));
      };
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

   // wait for server to get ready
   std::this_thread::sleep_for(200ms);

   simppl::dbus::Dispatcher d("bus:session");

   simppl::dbus::Stub<Errors> stub(d, "s");

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
      (void)res; //remove unused warning
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
