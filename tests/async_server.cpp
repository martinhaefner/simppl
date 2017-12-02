#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <thread>


using simppl::dbus::in;
using simppl::dbus::out;


namespace test {

INTERFACE(AsyncServer)
{
   // FIXME validate all parameters to be either in or out or oneway
   Method<in<int>, simppl::dbus::oneway> oneway;

   Method<in<int>, in<double>, out<double>> add;
   Method<in<int>, in<double>, out<int>, out<double>> echo;


   inline
   AsyncServer()
    : INIT(oneway)
    , INIT(add)
    , INIT(echo)
   {
      // NOOP
   }
};

}   // namespace

using namespace test;


namespace {


struct Client : simppl::dbus::Stub<AsyncServer>
{
   Client(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<AsyncServer>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);
      
         add.async(42, 777) >> [this](simppl::dbus::CallState s, double d){
            EXPECT_TRUE((bool)s);
            oneway(42);
         };
         
         echo.async(42, 3.1415) >> [this](simppl::dbus::CallState s, int i, double d){
            EXPECT_TRUE((bool)s);
            haveEcho_ = true;
         };
      };
   }

   bool haveEcho_ = false;
};


struct ShutdownClient : simppl::dbus::Stub<AsyncServer>
{
   ShutdownClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<AsyncServer>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(expected_, s);

         if (s == simppl::dbus::ConnectionState::Connected)
         {
            add.async(42, 777) >> [this](simppl::dbus::CallState s, double d){
               EXPECT_FALSE((bool)s);
               EXPECT_STREQ(s.exception().name(), "org.freedesktop.DBus.Error.NoReply");
      
               disp().stop();
            };
            
            oneway(42);
         }

         expected_ = simppl::dbus::ConnectionState::Disconnected;
         ++count_;
      };
   }

   simppl::dbus::ConnectionState expected_ = simppl::dbus::ConnectionState::Connected;
   int count_ = 0;
};


struct Server : simppl::dbus::Skeleton<AsyncServer>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<AsyncServer>(d, rolename)
   {
      oneway >> [this](int){
         disp().stop();
      };
      
      add >> [this](int i, double d){
         req_ = defer_response();
      };
      
      echo >> [this](int i, double d){
         respond_on(req_, add(d));
         respond_with(echo(i, d));
      };
   }

   simppl::dbus::ServerRequestDescriptor req_;
};


}   // anonymous namespace


/// one response can overtake another
TEST(AServer, trivial)
{
   simppl::dbus::Dispatcher d("bus:session");
   Client c(d);
   Server s(d, "s");

   d.run();

   EXPECT_TRUE(c.haveEcho_);
}


/// when shutting down a server, an outstanding response must be answered
/// with a transport error
TEST(AServer, outstanding)
{
   std::unique_ptr<simppl::dbus::Dispatcher> sd(new simppl::dbus::Dispatcher("bus:session"));
   simppl::dbus::Dispatcher cd;

   ShutdownClient c(cd);
   std::unique_ptr<Server> s(new Server(*sd, "s"));

   std::thread t([&sd, &s](){
      sd->run();

      s.reset();
      sd.reset();
   });

   cd.run();

   EXPECT_EQ(2, c.count_);

   t.join();
}
