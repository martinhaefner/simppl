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

INTERFACE(Simple)
{
   Request<> hello;
   Request<int> oneway;
   Request<int, double> add;
   Request<int, double> echo;

   Response<> world;
   Response<double> result;
   Response<int, double> rEcho;

   Attribute<int> data;

   Signal<int> sig;

   inline
   Simple()
    : INIT(hello)
    , INIT(oneway)
    , INIT(add)
    , INIT(echo)
    , INIT(world)
    , INIT(result)
    , INIT(rEcho)
    , INIT(data)
    , INIT(sig)
   {
      hello >> world;
      add >> result;
      echo >> rEcho;
   }
};

}

using namespace test;


namespace {


struct Client : simppl::dbus::Stub<Simple>
{
   Client()
    : simppl::dbus::Stub<Simple>("s", "unix:SimpleTest")
   {
      connected >> std::bind(&Client::handleConnected, this, _1);
      world >> std::bind(&Client::handleWorld, this, _1);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);
      hello();
   }


   void handleWorld(simppl::dbus::CallState state)
   {
      EXPECT_TRUE((bool)state);
      EXPECT_FALSE(state.isTransportError());
      EXPECT_FALSE(state.isRuntimeError());

      oneway(42);

      // shutdown
      oneway(7777);
   }
};


struct DisconnectClient : simppl::dbus::Stub<Simple>
{
   DisconnectClient()
    : simppl::dbus::Stub<Simple>("s", "unix:SimpleTest")
   {
      connected >> std::bind(&DisconnectClient::handleConnected, this, _1);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(expected_, s);

      if (s == simppl::dbus::ConnectionState::Connected)
      {
         oneway(7777);
      }
      else
         disp().stop();

      expected_ = simppl::dbus::ConnectionState::Disconnected;
   }

   simppl::dbus::ConnectionState expected_ = simppl::dbus::ConnectionState::Connected;
};


struct AttributeClient : simppl::dbus::Stub<Simple>
{
   AttributeClient()
    : simppl::dbus::Stub<Simple>("sa", "unix:SimpleTest")
   {
      connected >> std::bind(&AttributeClient::handleConnected, this, _1);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

      // like for signals, attributes must be attached when the client is connected
      data.attach() >> std::bind(&AttributeClient::attributeChanged, this, _1, _2);
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
   SignalClient()
    : simppl::dbus::Stub<Simple>("ss", "unix:SimpleTest")
   {
      connected >> std::bind(&SignalClient::handleConnected, this, _1);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

      // like for attributes, attributes must be attached when the client is connected
      sig.attach() >> std::bind(&SignalClient::handleSignal, this, _1);
      oneway(100);
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
   Server(const char* rolename)
    : simppl::dbus::Skeleton<Simple>(rolename)
   {
      hello >> std::bind(&Server::handleHello, this);
      oneway >> std::bind(&Server::handleOneway, this, _1);
      add >> std::bind(&Server::handleAdd, this, _1, _2);
      echo >> std::bind(&Server::handleEcho, this, _1, _2);

      // initialize attribute
      data = 4711;
   }

   void handleHello()
   {
      respondWith(world());
   }

   void handleOneway(int i)
   {
      ++count_oneway_;

      if (i == 7777)
      {
         disp().stop();
      }
      else if (i < 100)
      {
         EXPECT_EQ(42, i);
         data = 42;
      }
      else
         sig.notify(i);
   }

   void handleAdd(int i, double d)
   {
      respondWith(result(i*d));
   }

   void handleEcho(int i, double d)
   {
      respondWith(rEcho(i, d));
   }

   int count_oneway_ = 0;
};


}   // anonymous namespace


TEST(Simple, methods)
{
   simppl::dbus::Dispatcher d("bus:session");
   Client c;
   Server s("s");

   d.addClient(c);
   d.addServer(s);

   d.run();
}


TEST(Simple, signal)
{
   simppl::dbus::Dispatcher d("bus:session");
   SignalClient c;
   Server s("ss");

   d.addClient(c);
   d.addServer(s);

   d.run();

   EXPECT_EQ(c.count_, 10);
}


TEST(Simple, attribute)
{
   simppl::dbus::Dispatcher d("bus:session");
   AttributeClient c;
   Server s("sa");

   d.addClient(c);
   d.addServer(s);

   d.run();
}


TEST(Simple, blocking)
{
   simppl::dbus::Dispatcher d("bus:session");

   Server s("sb");
   d.addServer(s);

   simppl::dbus::Stub<Simple> stub("sb", "bus:session");
   d.addClient(stub);

   stub.connect();

   stub.oneway(101);
   stub.oneway(102);
   stub.oneway(103);

   double result;
   stub.add(42, 0.5) >> result;

   EXPECT_GT(21.01, result);
   EXPECT_LT(20.99, result);

   int i;
   double x;
   stub.echo(42, 0.5) >> std::tie(i, x);

   EXPECT_EQ(42, i);
   EXPECT_GT(0.51, x);
   EXPECT_LT(0.49, x);

   std::tuple<int, double> rslt;
   stub.echo(42, 0.5) >> rslt;

   EXPECT_EQ(42, std::get<0>(rslt));
   EXPECT_GT(0.51, std::get<1>(rslt));
   EXPECT_LT(0.49, std::get<1>(rslt));

   int dv = -1;
   stub.data.get() >> dv;
   EXPECT_EQ(4711, dv);
   EXPECT_EQ(4711, stub.data.value());

   EXPECT_EQ(3, s.count_oneway_);
}


TEST(Simple, disconnect)
{
   simppl::dbus::Dispatcher clientd;

   DisconnectClient c;
   clientd.addClient(c);

   {
      simppl::dbus::Dispatcher* serverd = new simppl::dbus::Dispatcher("bus:session");
      Server* s = new Server("s");
      serverd->addServer(*s);

      std::thread serverthread([serverd, s](){
         serverd->run();
         delete s;
         delete serverd;
      });

      clientd.run();

      serverthread.join();
   }
}
