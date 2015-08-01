#include <gtest/gtest.h>

// for new call semantic on blocking functions
#define SIMPPL_HAVE_BOOST_FUSION 1

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <thread>


using namespace std::placeholders;


INTERFACE(Simple)
{   
   Request<> hello;
   Request<int> oneway;
   Request<int, double> add;
   
   Response<> world;
   Response<double> result;
   
   Attribute<int> data;
   
   Signal<int> sig;
   
   inline
   Simple()
    : INIT_REQUEST(hello)
    , INIT_REQUEST(oneway)
    , INIT_REQUEST(add)
    , INIT_RESPONSE(world)
    , INIT_RESPONSE(result)
    , INIT_ATTRIBUTE(data)
    , INIT_SIGNAL(sig)
   {
      hello >> world;
      add >> result;
   }
};


namespace {
   

struct Client : simppl::ipc::Stub<Simple>
{
   Client()   
    : simppl::ipc::Stub<Simple>("s", "unix:SimpleTest")    
   {
      connected >> std::bind(&Client::handleConnected, this, _1);
      world >> std::bind(&Client::handleWorld, this, _1);
   }
   
   
   void handleConnected(simppl::ipc::ConnectionState s)
   {
      EXPECT_EQ(simppl::ipc::ConnectionState::Connected, s);
      hello();
   }
   
   
   void handleWorld(const simppl::ipc::CallState& state)
   {
      EXPECT_TRUE((bool)state);
      EXPECT_FALSE(state.isTransportError());
      EXPECT_FALSE(state.isRuntimeError());
      
      oneway(42);
      
      // shutdown
      oneway(7777);
   }
};


struct AttributeClient : simppl::ipc::Stub<Simple>
{
   AttributeClient()   
    : simppl::ipc::Stub<Simple>("sa", "unix:SimpleTest")
   {
      connected >> std::bind(&AttributeClient::handleConnected, this, _1);
   }
   
   
   void handleConnected(simppl::ipc::ConnectionState s)
   {
      EXPECT_EQ(simppl::ipc::ConnectionState::Connected, s);
      
      // like for signals, attributes must be attached when the client is connected
      data.attach() >> std::bind(&AttributeClient::attributeChanged, this, _1);
   }
   
   
   void attributeChanged(int new_value)
   {
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


struct SignalClient : simppl::ipc::Stub<Simple>
{
   SignalClient()   
    : simppl::ipc::Stub<Simple>("ss", "unix:SimpleTest")    
   {
      connected >> std::bind(&SignalClient::handleConnected, this, _1);
   }
   
   
   void handleConnected(simppl::ipc::ConnectionState s)
   {
      EXPECT_EQ(simppl::ipc::ConnectionState::Connected, s);
      
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


struct Server : simppl::ipc::Skeleton<Simple>
{
   Server(const char* rolename)
    : simppl::ipc::Skeleton<Simple>(rolename)
   {
      hello >> std::bind(&Server::handleHello, this);
      oneway >> std::bind(&Server::handleOneway, this, _1);
      add >> std::bind(&Server::handleAdd, this, _1, _2);
      
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
         sig.emit(i);
   }
   
   void handleAdd(int i, double d)
   {
      respondWith(result(i*d));
   }
   
   int count_oneway_ = 0;
};


}   // anonymous namespace


TEST(Simple, methods) 
{
   simppl::ipc::Dispatcher d("unix:SimpleTest");
   Client c;
   Server s("s");
   
   d.addClient(c);
   d.addServer(s);
   
   d.run();
}


TEST(Simple, signal) 
{
   simppl::ipc::Dispatcher d("unix:SimpleTest");
   SignalClient c;
   Server s("ss");
   
   d.addClient(c);
   d.addServer(s);
   
   d.run();
   
   EXPECT_EQ(c.count_, 10);
}


TEST(Simple, attribute) 
{
   simppl::ipc::Dispatcher d("unix:SimpleTest");
   AttributeClient c;
   Server s("sa");
   
   d.addClient(c);
   d.addServer(s);
   
   d.run();
}


TEST(Simple, blocking) 
{
   simppl::ipc::Dispatcher d("unix:SimpleTest");
   Server s("sb");
   d.addServer(s);
   
   simppl::ipc::Stub<Simple> stub("sb", "unix:SimpleTest");
   
   // FIXME if not added we get a segmentation fault -> add an assertion somewhere...
   d.addClient(stub);
   
   EXPECT_EQ(true, stub.connect());
   
   stub.oneway(101);
   stub.oneway(102);
   stub.oneway(103);

   double result;
   stub.add(42, 0.5) >> result;
   
   EXPECT_GT(21.01, result);
   EXPECT_LT(20.99, result);
   
   EXPECT_EQ(3, s.count_oneway_);
}


// FIXME add exceptions tests, event-driven and blocking
