#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <thread>


using namespace std::placeholders;

using simppl::dbus::in;
using simppl::dbus::out;


namespace test {

INTERFACE(AsyncServer)
{
   // FIXME validate all parameters to be either in or out or oneway
   Request<in<int>, simppl::dbus::oneway> oneway;

   Request<in<int>, in<double>, out<double>> add;
   Request<in<int>, in<double>, out<int>, out<double>> echo;


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
      connected >> std::bind(&Client::handleConnected, this, _1);
      add >> std::bind(&Client::handleAdd, this, _1, _2);
      echo >> std::bind(&Client::handleEcho, this, _1, _2, _3);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);
      add.async(42, 777);
      echo.async(42, 3.1415);
    }


   void handleAdd(simppl::dbus::CallState s, double d)
   {
      EXPECT_TRUE((bool)s);
      oneway(42);
   }


   void handleEcho(simppl::dbus::CallState s, int i, double d)
   {
      EXPECT_TRUE((bool)s);
      haveEcho_ = true;
   }

   bool haveEcho_ = false;
};


struct ShutdownClient : simppl::dbus::Stub<AsyncServer>
{
   ShutdownClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<AsyncServer>(d, "s")
   {
      connected >> std::bind(&ShutdownClient::handleConnected, this, _1);
      add >> std::bind(&ShutdownClient::handleResult, this, _1, _2);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(expected_, s);

      if (s == simppl::dbus::ConnectionState::Connected)
      {
         add.async(42, 777);
         oneway(42);
      }

      expected_ = simppl::dbus::ConnectionState::Disconnected;
      ++count_;
   }


   void handleResult(simppl::dbus::CallState s, double d)
   {
      EXPECT_FALSE((bool)s);
      EXPECT_STREQ(s.exception().name(), "org.freedesktop.DBus.Error.NoReply");

      disp().stop();
   }

   simppl::dbus::ConnectionState expected_ = simppl::dbus::ConnectionState::Connected;
   int count_ = 0;
};


struct Server : simppl::dbus::Skeleton<AsyncServer>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<AsyncServer>(d, rolename)
   {
      oneway >> std::bind(&Server::handleOneway, this, _1);
      add >> std::bind(&Server::handleAdd, this, _1, _2);
      echo >> std::bind(&Server::handleEcho, this, _1, _2);
   }

   void handleOneway(int)
   {
      disp().stop();
   }

   void handleAdd(int i, double d)
   {
      req_ = deferResponse();
   }

   void handleEcho(int i, double d)
   {
      respondOn(req_, add(d));
      respondWith(echo(i, d));
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
