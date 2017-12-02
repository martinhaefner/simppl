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

INTERFACE(Timeout)
{
   Method<in<int>, out<double>> eval;
   Method<in<int>, simppl::dbus::oneway> oneway;

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
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(expect_, s);

         if (s == simppl::dbus::ConnectionState::Connected)
         {
            start_ = std::chrono::steady_clock::now();
            
            eval.async(42) >> [this](simppl::dbus::CallState state, double){
               EXPECT_FALSE((bool)state);

               EXPECT_STREQ(state.exception().name(), "org.freedesktop.DBus.Error.NoReply");

               long millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_).count();
               EXPECT_GE(millis, 500);
               EXPECT_LT(millis, 600);

               gbl_disp->stop();   // servers dispatcher
               disp().stop();
            };
         }

         expect_ = simppl::dbus::ConnectionState::Disconnected;
      };
   }

   std::chrono::steady_clock::time_point start_;

   simppl::dbus::ConnectionState expect_ = simppl::dbus::ConnectionState::Connected;
};


struct DisconnectClient : simppl::dbus::Stub<Timeout>
{
   DisconnectClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Timeout>(d, "tm")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(expect_, s);

         if (s == simppl::dbus::ConnectionState::Connected)
         {
            eval.async(777) >> [this](simppl::dbus::CallState state, double){
               EXPECT_FALSE((bool)state);
               EXPECT_STREQ(state.exception().name(), "org.freedesktop.DBus.Error.Timeout");

               disp().stop();
            };
         }

         expect_ = simppl::dbus::ConnectionState::Disconnected;
      };
   }

   simppl::dbus::ConnectionState expect_ = simppl::dbus::ConnectionState::Connected;
};


struct OnewayClient : simppl::dbus::Stub<Timeout>
{
   OnewayClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Timeout>(d, "tm")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(expect_, s);

         if (s == simppl::dbus::ConnectionState::Connected)
         {
            gbl_disp = &disp();
            oneway(42);
         }

         expect_ = simppl::dbus::ConnectionState::Disconnected;
      };
   }

   simppl::dbus::ConnectionState expect_ = simppl::dbus::ConnectionState::Connected;
};


struct NeverConnectedClient : simppl::dbus::Stub<Timeout>
{
   NeverConnectedClient(simppl::dbus::Dispatcher& d, simppl::dbus::ConnectionState expected = simppl::dbus::ConnectionState::Timeout)
    : simppl::dbus::Stub<Timeout>(d, "tm")
    , expected_(expected)
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(expected_, s);
         disp().stop();
      };
   }

   simppl::dbus::ConnectionState expected_;
};


struct Server : simppl::dbus::Skeleton<Timeout>
{
   Server(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Skeleton<Timeout>(d, "tm")
   {
      eval >> [this](int i){
        // generate timeout on client side
         std::this_thread::sleep_for(1s);

         if (i == 42)
         {
            respond_with(eval(3.1415));
         }
         else
            (void)defer_response();
      };
      
      oneway >> [this](int i){
         // generate timeout on client side
         std::this_thread::sleep_for(1s);

         disp().stop();
         gbl_disp->stop();    // clients dispatcher
      };
   }
};


void runServer()
{
   simppl::dbus::Dispatcher d("bus:session");
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


/*void runClient()
{
   simppl::dbus::Dispatcher d;
   DisconnectClient c(d);

   d.run();
}
*/

}   // anonymous namespace


TEST(Timeout, method)
{
   simppl::dbus::enable_threads();
   
   std::thread serverthread(&runServer);

   simppl::dbus::Dispatcher d;
   Client c(d);

   d.set_request_timeout(500ms);

   d.run();

   serverthread.join();
}


TEST(Timeout, oneway)
{
   std::thread serverthread(&runServer);

   simppl::dbus::Dispatcher d;
   OnewayClient c(d);

   d.set_request_timeout(500ms);

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

   simppl::dbus::Dispatcher d("bus:session");
   simppl::dbus::Stub<Timeout> stub(d, "tm");

   // wait for server to get ready
   std::this_thread::sleep_for(200ms);

   // default timeout
   d.set_request_timeout(500ms);
   
   auto start = std::chrono::steady_clock::now();

   try
   {
      // request specific timeout -> overrides default
      double rc = stub.eval[simppl::dbus::timeout = 700ms](42);
      (void)rc;  //remove unused warnings
      // never arrive here!
      EXPECT_FALSE(true);
   }
   catch(simppl::dbus::Error& err)
   {
      EXPECT_STREQ("org.freedesktop.DBus.Error.NoReply", err.name());
   }

   long millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
   EXPECT_GE(millis, 700);
   EXPECT_LT(millis, 750);

   // cleanup server
   gbl_disp->stop();
   serverthread.join();
}


TEST(Timeout, blocking_api)
{
   std::thread serverthread(&runServer);

   simppl::dbus::Dispatcher d;
   simppl::dbus::Stub<Timeout> stub(d, "tm");

   // wait for server to get ready
   std::this_thread::sleep_for(200ms);

   d.set_request_timeout(500ms);

   try
   {
      double rc = stub.eval(42);
      (void)rc; // remove unused warnings
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

