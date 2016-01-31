#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/interface.h"
#include "simppl/dispatcher.h"
#include "simppl/blocking.h"

#include <thread>
#include <chrono>

using namespace std::placeholders;

namespace spl = simppl::ipc;

using namespace std::literals::chrono_literals;


namespace test
{

INTERFACE(Simple)
{
   Request<> oneway;
   
   Request<std::string> echo;
   Response<std::string> rEcho;
   
   Signal<> sig;
   
   Simple()
    : INIT_REQUEST(oneway)
    , INIT_REQUEST(echo)
    , INIT_RESPONSE(rEcho)
    , INIT_SIGNAL(sig)
   {
      echo >> rEcho;
   }
};

}   // namespace


// ---------------------------------------------------------------------

//#define BLOCKING_ONEWAY
#define TEST_SIG

struct Client : spl::Stub<test::Simple>
{
   Client()
    : spl::Stub<test::Simple>("mysimple")
   {
      connected >> std::bind(&Client::handle_conn, this, _1);
      rEcho >> std::bind(&Client::handle_recho, this, _1, _2);
   }
   
   void handle_conn(spl::ConnectionState s)
   {
      if (s == spl::ConnectionState::Connected)
      {
#ifdef TEST_SIG      
         sig.attach() >> std::bind(&Client::handle_sig, this);
         oneway();
#else
         echo("Hallo Welt");
#endif
      }
   }
   
   void handle_recho(spl::CallState c, const std::string& str)
   {
      echo("Hallo Welt");
   }
   
   void handle_sig()
   {
      oneway();  
   }
};


int client()
{
   Client c;
   
   spl::Dispatcher disp("dbus:session");
   disp.addClient(c);

#ifdef BLOCKING_ONEWAY
   c.connect();

   while(true)
   {
      c.oneway();
      std::this_thread::sleep_for(50ms);
   }
#else
   disp.run();
#endif
   
}


// ---------------------------------------------------------------------


struct SimpleServer : spl::Skeleton<test::Simple>
{
    SimpleServer()
     : spl::Skeleton<test::Simple>("mysimple")
    {
        oneway >> std::bind(&SimpleServer::handle_oneway, this);
        echo >> std::bind(&SimpleServer::handle_echo, this, _1);
    }


    void handle_oneway()
    {
        static int count = 0;

        if (++count % 20 == 0)
           std::cout << "Client saying " << count << std::endl;
           
        sig.emit();
    }
    
    
    void handle_echo(const std::string& str)
    {
       static int cnt = 0;
       
       if (++cnt % 20 == 0)
         std::cout << "Echo " << str << " " << cnt << std::endl;
         
       respondWith(rEcho("Super"));
    }
};


int server()
{
   SimpleServer serv;

   spl::Dispatcher disp("dbus:session");
   disp.addServer(serv);

   return disp.run();
}


int main(int argc, char** argv)
{
    return argc > 1 ? server() : client();
}
