#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <thread>


using namespace std::literals::chrono_literals;
using namespace std::placeholders;

using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{

INTERFACE(Timeout)
{
   Request<in<int>, out<double>> eval;
   Request<in<int>, simppl::dbus::Oneway> oneway;

   inline
   Timeout()
    : INIT(eval)
    , INIT(oneway)
   {
      // NOOP
   }
};

}   // namespace

using namespace test;


namespace {

simppl::dbus::Dispatcher* gbl_disp = nullptr;


struct Client : simppl::dbus::Stub<Timeout>
{
   Client(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Timeout>(d, "tm")
   {
      connected >> std::bind(&Client::handleConnected, this, _1);
      eval >> std::bind(&Client::handleEval, this, _1, _2);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(expect_, s);

      if (s == simppl::dbus::ConnectionState::Connected)
      {
         start_ = std::chrono::steady_clock::now();
         eval.async(42);
      }

      expect_ = simppl::dbus::ConnectionState::Disconnected;
   }


   void handleEval(simppl::dbus::CallState state, double)
   {
      EXPECT_FALSE((bool)state);

      EXPECT_STREQ(state.exception().name(), "org.freedesktop.DBus.Error.NoReply");

      int millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_).count();
      EXPECT_GE(millis, 500);
      EXPECT_LT(millis, 600);

      gbl_disp->stop();   // servers dispatcher
      disp().stop();
   }

   std::chrono::steady_clock::time_point start_;

   simppl::dbus::ConnectionState expect_ = simppl::dbus::ConnectionState::Connected;
};


struct DisconnectClient : simppl::dbus::Stub<Timeout>
{
   DisconnectClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Timeout>(d, "tm")
   {
      connected >> std::bind(&DisconnectClient::handleConnected, this, _1);
      eval >> std::bind(&DisconnectClient::handleEval, this, _1, _2);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(expect_, s);

      if (s == simppl::dbus::ConnectionState::Connected)
         eval.async(777);

      expect_ = simppl::dbus::ConnectionState::Disconnected;
   }


   void handleEval(simppl::dbus::CallState state, double)
   {
      EXPECT_FALSE((bool)state);
      EXPECT_STREQ(state.exception().name(), "org.freedesktop.DBus.Error.Timeout");

      disp().stop();
   }

   simppl::dbus::ConnectionState expect_ = simppl::dbus::ConnectionState::Connected;
};


struct OnewayClient : simppl::dbus::Stub<Timeout>
{
   OnewayClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Timeout>(d, "tm")
   {
      connected >> std::bind(&OnewayClient::handleConnected, this, _1);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(expect_, s);

      if (s == simppl::dbus::ConnectionState::Connected)
      {
         gbl_disp = &disp();
         oneway(42);
      }

      expect_ = simppl::dbus::ConnectionState::Disconnected;
   }

   simppl::dbus::ConnectionState expect_ = simppl::dbus::ConnectionState::Connected;
};


struct NeverConnectedClient : simppl::dbus::Stub<Timeout>
{
   NeverConnectedClient(simppl::dbus::Dispatcher& d, simppl::dbus::ConnectionState expected = simppl::dbus::ConnectionState::Timeout)
    : simppl::dbus::Stub<Timeout>(d, "tm")
    , expected_(expected)
   {
      connected >> std::bind(&NeverConnectedClient::handleConnected, this, _1);
   }

   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(expected_, s);
      disp().stop();
   }

   simppl::dbus::ConnectionState expected_;
};


struct Server : simppl::dbus::Skeleton<Timeout>
{
   Server(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Skeleton<Timeout>(d, "tm")
   {
      eval >> std::bind(&Server::handleEval, this, _1);
      oneway >> std::bind(&Server::handleOneway, this, _1);
   }

   void handleEval(int i)
   {
//      std::cout << "handleeval" << std::endl;
      // generate timeout on client side
      std::this_thread::sleep_for(1s);

      if (i == 42)
      {
  //       std::cout << "response" << std::endl;
         respondWith(eval(3.1415));
      }
      else
         (void)deferResponse();
   }

   void handleOneway(int i)
   {
      // generate timeout on client side
      std::this_thread::sleep_for(1s);

      disp().stop();
      gbl_disp->stop();    // clients dispatcher
   }
};


void runServer()
{
   simppl::dbus::Dispatcher d("dbus:session");
   gbl_disp = &d;

   try
   {
      Server s(d);

      d.run();
   }
   catch(...)
   {
      std::cout << "Ex. in server" << std::endl;
   }
}


void runClient()
{
   simppl::dbus::Dispatcher d;
   DisconnectClient c(d);

   d.run();
}

}   // anonymous namespace


TEST(Timeout, method)
{
   std::thread serverthread(&runServer);

   simppl::dbus::Dispatcher d;
   Client c(d);

   d.setRequestTimeout(500ms);

   d.run();

   serverthread.join();
}


TEST(Timeout, oneway)
{
   std::thread serverthread(&runServer);

   simppl::dbus::Dispatcher d;
   OnewayClient c(d);

   d.setRequestTimeout(500ms);

   d.run();

   serverthread.join();
}


/* FIXME TEST(Timeout, no_timeout)
{
   std::thread serverthread(&runServer);
   std::thread clientthread(&runClient);

   std::this_thread::sleep_for(700ms);
   gbl_disp->stop();

   serverthread.join();
   clientthread.join();
}*/


TEST(Timeout, request_specific)
{
   std::thread serverthread(&runServer);

   simppl::dbus::Dispatcher d("dbus:session");
   simppl::dbus::Stub<Timeout> stub(d, "tm");

   // default timeout
   d.setRequestTimeout(500ms);

   stub.connect();

   auto start = std::chrono::steady_clock::now();

   try
   {
      // request specific timeout -> overrides default
      double rc = stub.eval[simppl::dbus::timeout = 700ms](42);

      // never arrive here!
      EXPECT_FALSE(true);
   }
   catch(simppl::dbus::Error& err)
   {
      EXPECT_STREQ(err.name(), "org.freedesktop.DBus.Error.NoReply");
   }

   int millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
   EXPECT_GE(millis, 700);
   EXPECT_LT(millis, 750);

   // cleanup server
   gbl_disp->stop();
   serverthread.join();
}


TEST(Timeout, blocking_connect)
{
   simppl::dbus::Dispatcher d;
   simppl::dbus::Stub<Timeout> stub(d, "tm");

   d.setRequestTimeout(500ms);

   try
   {
      stub.connect();

      // never arrive here!
      EXPECT_FALSE(true);
   }
   catch(const simppl::dbus::Error& err)
   {
      EXPECT_STREQ(err.name(), "org.freedesktop.DBus.Error.Timeout");
   }
}


TEST(Timeout, blocking_api)
{
   std::thread serverthread(&runServer);

   simppl::dbus::Dispatcher d;
   simppl::dbus::Stub<Timeout> stub(d, "tm");

   d.setRequestTimeout(500ms);

   stub.connect();

   try
   {
      double rc = stub.eval(42);

      // never arrive here!
      EXPECT_FALSE(true);
   }
   catch(const simppl::dbus::Error& err)
   {
      EXPECT_STREQ(err.name(), "org.freedesktop.DBus.Error.NoReply");
   }

   // cleanup server
   gbl_disp->stop();
   serverthread.join();
}

