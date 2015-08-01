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
   
   Response<double> rEval;
   
   inline
   Timeout()
    : INIT_REQUEST(eval)
    , INIT_REQUEST(oneway)
    , INIT_RESPONSE(rEval)
   {
      eval >> rEval;
   }
};


namespace {
   
simppl::ipc::Dispatcher* gbl_disp = nullptr;


struct Client : simppl::ipc::Stub<Timeout>
{
   Client()   
    : simppl::ipc::Stub<Timeout>("tm", "unix:TimeoutTest")    
   {
      connected >> std::bind(&Client::handleConnected, this, _1);
      rEval >> std::bind(&Client::handleEval, this, _1, _2);
   }
   
   
   void handleConnected(simppl::ipc::ConnectionState s)
   {
      EXPECT_EQ(simppl::ipc::ConnectionState::Connected, s);
      
      eval(42);
   }
   
   
   void handleEval(const simppl::ipc::CallState& state, double)
   {
      EXPECT_FALSE((bool)state);
      
      EXPECT_TRUE(state.isTransportError());
      EXPECT_FALSE(state.isRuntimeError());
      EXPECT_EQ(ETIMEDOUT, static_cast<const simppl::ipc::TransportError&>(state.exception()).getErrno());
      
      gbl_disp->stop();   // servers dispatcher
      disp().stop();
   }
};


struct DisconnectClient : simppl::ipc::Stub<Timeout>
{
   DisconnectClient()   
    : simppl::ipc::Stub<Timeout>("tm", "unix:TimeoutTest")    
   {
      connected >> std::bind(&DisconnectClient::handleConnected, this, _1);
      rEval >> std::bind(&DisconnectClient::handleEval, this, _1, _2);
   }
   
   
   void handleConnected(simppl::ipc::ConnectionState s)
   {
      EXPECT_EQ(simppl::ipc::ConnectionState::Connected, s);
      
      eval(777);
   }
   
   
   void handleEval(const simppl::ipc::CallState& state, double)
   {
      EXPECT_FALSE((bool)state);
      
      EXPECT_TRUE(state.isTransportError());
      EXPECT_EQ(ECONNABORTED, static_cast<const simppl::ipc::TransportError&>(state.exception()).getErrno());
      EXPECT_FALSE(state.isRuntimeError());
      
      disp().stop();
   }
};


struct OnewayClient : simppl::ipc::Stub<Timeout>
{
   OnewayClient()   
    : simppl::ipc::Stub<Timeout>("tm", "unix:TimeoutTest")    
   {
      connected >> std::bind(&OnewayClient::handleConnected, this, _1);
   }
   
   
   void handleConnected(simppl::ipc::ConnectionState s)
   {
      EXPECT_EQ(simppl::ipc::ConnectionState::Connected, s);
      
      gbl_disp = &disp();
      oneway(42);
   }
};


struct NeverConnectedClient : simppl::ipc::Stub<Timeout>
{
   NeverConnectedClient(simppl::ipc::ConnectionState expected = simppl::ipc::ConnectionState::Timeout)   
    : simppl::ipc::Stub<Timeout>("tm", "unix:TimeoutTest")   
    , expected_(expected) 
   {
      connected >> std::bind(&NeverConnectedClient::handleConnected, this, _1);
   }
   
   void handleConnected(simppl::ipc::ConnectionState s)
   {
      EXPECT_EQ(expected_, s);
      disp().stop();
   }
   
   simppl::ipc::ConnectionState expected_;
};


struct Server : simppl::ipc::Skeleton<Timeout>
{
   Server()
    : simppl::ipc::Skeleton<Timeout>("tm")
   {
      eval >> std::bind(&Server::handleEval, this, _1);
      oneway >> std::bind(&Server::handleOneway, this, _1);
   }
   
   void handleEval(int i)
   {
      // generate timeout on client side
      std::this_thread::sleep_for(std::chrono::seconds(1));
   
      if (i == 42)
      {
         respondWith(rEval(3.1415));
      }
      else
         (void)deferResponse();
   }
   
   void handleOneway(int i)
   {
      // generate timeout on client side
      std::this_thread::sleep_for(std::chrono::seconds(1));
      
      disp().stop();
      gbl_disp->stop();    // clients dispatcher
   }
};


void runServer()
{
   simppl::ipc::Dispatcher d("unix:TimeoutTest");
   gbl_disp = &d;
   
   Server s;
   d.addServer(s);
   
   d.run();
}


void runClient()
{
   simppl::ipc::Dispatcher d;
   DisconnectClient c;
   
   d.addClient(c);
   d.run();
}

}   // anonymous namespace


TEST(Timeout, method) 
{
   std::thread serverthread(&runServer);
   
   simppl::ipc::Dispatcher d;
   Client c;
   
   d.setRequestTimeout(std::chrono::milliseconds(500));
   
   d.addClient(c);
   d.run();
   
   serverthread.join();
}


TEST(Timeout, oneway) 
{
   std::thread serverthread(&runServer);
   
   simppl::ipc::Dispatcher d;
   OnewayClient c;
   
   d.setRequestTimeout(std::chrono::milliseconds(500));
   
   d.addClient(c);
   d.run();
   
   serverthread.join();
}


TEST(Timeout, no_timeout) 
{
   std::thread serverthread(&runServer);
   std::thread clientthread(&runClient);
   
   std::this_thread::sleep_for(std::chrono::milliseconds(700));
   gbl_disp->stop();
   
   // FIXME client must get a wakeup that it will never be answered
   
   serverthread.join();
   clientthread.join();
}


TEST(Timeout, request_specific) 
{
   // FIXME
}


TEST(Timeout, blocking_api)
{
   // FIXME
}


TEST(Timeout, interface_attach_invalid_server)
{
   simppl::ipc::Dispatcher d("unix:TimeoutTest");
   NeverConnectedClient c(simppl::ipc::ConnectionState::NotAvailable);
   
   d.addClient(c);
   d.run();
}


TEST(Timeout, interface_attach)
{
   // server dispatcher not running ant therefore never returning response frame
   simppl::ipc::Dispatcher d("unix:TimeoutTest");
   simppl::ipc::Dispatcher d_client;
   NeverConnectedClient c;
   
   d_client.addClient(c);
   d_client.run();
}


TEST(Timeout, inotify)
{
   simppl::ipc::Dispatcher d;
   NeverConnectedClient c;
   
   d.addClient(c);
   d.run();
}
