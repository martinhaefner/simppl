#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <thread>


using namespace std::placeholders;


INTERFACE(Timeout)
{   
   Request<int> eval;
   Request<int> oneway;
   
   // FIXME do we support empty responses now?
   Response<double> rEval;
   
   inline
   Timeout()
    : INIT_REQUEST(eval)
    , INIT_REQUEST(oneway)   // FIXME must ignore timeout here!
    , INIT_RESPONSE(rEval)
   {
      eval >> rEval;
   }
};


namespace {
   
simppl::ipc::Dispatcher* serverd = nullptr;


struct Client : simppl::ipc::Stub<Timeout>
{
   Client(int calls = 1)   
    : simppl::ipc::Stub<Timeout>("tm", "unix:TimeoutTest")    
   {
      connected >> std::bind(&Client::handleConnected, this);
      rEval >> std::bind(&Client::handleEval, this, _1, _2);
   }
   
   
   void handleConnected()
   {
      eval(42);
   }
   
   
   void handleEval(const simppl::ipc::CallState& state, double d)
   {
      EXPECT_FALSE((bool)state);
      
      EXPECT_TRUE(state.isTransportError());
      EXPECT_FALSE(state.isRuntimeError());
      
      serverd->stop();
      disp().stop();
   }
};


struct Server : simppl::ipc::Skeleton<Timeout>
{
   Server()
    : simppl::ipc::Skeleton<Timeout>("tm")
   {
      eval >> std::bind(&Server::handleEval, this, _1);
   }
   
   void handleEval(int i)
   {
      std::this_thread::sleep_for(std::chrono::seconds(1));
   
      double d = 3.1415;   // FIXME must allow rvalues here!
      respondWith(rEval(d));
   }
};


void runServer()
{
   simppl::ipc::Dispatcher d("unix:TimeoutTest");
   serverd = &d;
   
   Server s;
   d.addServer(s);
   
   d.run();
}

}   // anonymous namespace


TEST(Timeout, method) 
{
   std::thread serverthread(&runServer);
   
   // FIXME must make sure we don't need this wait here! -> inotify or anything else...
   std::this_thread::sleep_for(std::chrono::milliseconds(300));
   
   simppl::ipc::Dispatcher d;
   Client c;
   
   d.setRequestTimeout(std::chrono::milliseconds(500));
   
   d.addClient(c);
   d.run();
   
   std::cout << "Finished" << std::endl;
   serverthread.join();
}


TEST(Timeout, oneway) 
{
   // FIXME
}


TEST(Timeout, no_timeout) 
{
   // FIXME
}


TEST(Timeout, request_specific) 
{
   // FIXME
}
