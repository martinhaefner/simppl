#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

using simppl::dbus::in;
using simppl::dbus::out;

#include <thread>


using namespace std::placeholders;


namespace test
{

INTERFACE(Simple)
{
   Request<> hello;

   Request<in<int>, simppl::dbus::oneway> oneway;

   Request<in<int>, in<double>, out<double>> add;
   Request<in<int>, in<double>, out<int>, out<double>> echo;

   Attribute<int> data;

   Signal<int> sig;

   inline
   Simple()
    : INIT(hello)
    , INIT(oneway)
    , INIT(add)
    , INIT(echo)
    , INIT(data)
    , INIT(sig)
   {
      // NOOP
   }
};

}

using namespace test;


namespace {


struct Client : simppl::dbus::Stub<Simple>
{
   Client(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Simple>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s)
      {
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);
         this->hello.async();
      };

      hello >> [this](simppl::dbus::CallState state)
      {
         EXPECT_TRUE((bool)state);

         this->oneway(42);

         // shutdown
         this->oneway(7777);
      };
   }
};


struct DisconnectClient : simppl::dbus::Stub<Simple>
{
   DisconnectClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Simple>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s)
      {
         EXPECT_EQ(this->expected_, s);

         if (s == simppl::dbus::ConnectionState::Connected)
         {
            this->oneway(7777);
         }
         else
            this->disp().stop();

         this->expected_ = simppl::dbus::ConnectionState::Disconnected;
      };
   }


   simppl::dbus::ConnectionState expected_ = simppl::dbus::ConnectionState::Connected;
};


struct AttributeClient : simppl::dbus::Stub<Simple>
{
   AttributeClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Simple>(d, "sa")
   {
      connected >> [this](simppl::dbus::ConnectionState s)
      {
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         // like for signals, attributes must be attached when the client is connected
         this->data.attach() >> [this](simppl::dbus::CallState state, int new_value)
         {
            this->attributeChanged(state, new_value);
         };
      };
   }

   void attributeChanged(simppl::dbus::CallState state, int new_value)
   {
      EXPECT_TRUE((bool)state);

      if (first_)
      {
         // first you get the current value
         EXPECT_EQ(4711, new_value);
         EXPECT_EQ(4711, data.value());

         // now we trigger update on server
         oneway(42);
      }
      else
      {
         EXPECT_EQ(42, new_value);
         EXPECT_EQ(42, data.value());

         oneway(7777);   // stop signal
      }

      first_ = false;
   }

   bool first_ = true;
};


struct SignalClient : simppl::dbus::Stub<Simple>
{
   SignalClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Simple>(d, "ss")
   {
      connected >> [this](simppl::dbus::ConnectionState s)
      {
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         // like for attributes, attributes must be attached when the client is connected
         this->sig.attach() >> [this](int value)
         {
            this->handleSignal(value);
         };

         this->oneway(100);
      };
   }


   void handleSignal(int value)
   {
      ++count_;

      EXPECT_EQ(start_++, value);

      // send stopsignal if appropriate
      if (start_ == 110)
         start_ = 7777;

      // trigger again
      oneway(start_);
   }

   int start_ = 100;
   int count_ = 0;
};


struct Server : simppl::dbus::Skeleton<Simple>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Simple>(d, rolename)
   {
      // initialize attribute
      data = 4711;

      // initialize handlers
      hello >> [this]()
      {
         this->respondWith(hello());
      };


      oneway >> [this](int i)
      {
         ++this->count_oneway_;

         if (i == 7777)
         {
            this->disp().stop();
         }
         else if (i < 100)
         {
            EXPECT_EQ(42, i);
            this->data = 42;
         }
         else
            this->sig.emit(i);
      };


      add >> [this](int i, double d)
      {
         this->respondWith(add(i*d));
      };


      echo >> [this](int i, double d)
      {
         this->respondWith(echo(i, d));
      };
   }

   int count_oneway_ = 0;
};


}   // anonymous namespace


TEST(Simple, methods)
{
   simppl::dbus::Dispatcher d("bus:session");
   Client c(d);
   Server s(d, "s");

   d.run();
}


TEST(Simple, signal)
{
   simppl::dbus::Dispatcher d("bus:session");
   SignalClient c(d);
   Server s(d, "ss");

   d.run();

   EXPECT_EQ(c.count_, 10);
}


TEST(Simple, attribute)
{
   simppl::dbus::Dispatcher d("bus:session");
   AttributeClient c(d);
   Server s(d, "sa");

   d.run();
}


void blockrunner()
{
   simppl::dbus::Dispatcher d("bus:session");

   Server s(d, "sb");

   d.run();

   EXPECT_EQ(4, s.count_oneway_);
}


TEST(Simple, blocking)
{
   simppl::dbus::Dispatcher d("bus:session");

   std::thread t(blockrunner);

   simppl::dbus::Stub<Simple> stub(d, "sb");

   stub.connect();

   stub.oneway(101);
   stub.oneway(102);
   stub.oneway(103);

   double result;
   result = stub.add(42, 0.5);

   EXPECT_GT(21.01, result);
   EXPECT_LT(20.99, result);

   int i;
   double x;
   std::tie(i, x) = stub.echo(42, 0.5);

   EXPECT_EQ(42, i);
   EXPECT_GT(0.51, x);
   EXPECT_LT(0.49, x);

   std::tuple<int, double> rslt;
   rslt = stub.echo(42, 0.5);

   EXPECT_EQ(42, std::get<0>(rslt));
   EXPECT_GT(0.51, std::get<1>(rslt));
   EXPECT_LT(0.49, std::get<1>(rslt));

   int dv = -1;
   dv = stub.data.get();
   EXPECT_EQ(4711, dv);
   EXPECT_EQ(4711, stub.data.value());

   stub.oneway(7777);   // stop server
   t.join();
}


TEST(Simple, disconnect)
{
   simppl::dbus::Dispatcher clientd;

   DisconnectClient c(clientd);

   {
      simppl::dbus::Dispatcher* serverd = new simppl::dbus::Dispatcher("bus:session");
      Server* s = new Server(*serverd, "s");

      std::thread serverthread([serverd, s](){
         serverd->run();
         delete s;
         delete serverd;
      });

      clientd.run();

      serverthread.join();
   }
}
