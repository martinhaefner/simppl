#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/interface.h"
#include "simppl/dispatcher.h"


using namespace std::placeholders;

namespace spl = simppl::ipc;


namespace test
{
    
INTERFACE(Simple)
{
   Request<int> echo;
   Response<int> rEcho;
   
   Signal<double> sigUsr;
   
   Attribute<int> attInt;
   
   Simple()
    : INIT_REQUEST(echo)
    , INIT_SIGNAL(sigUsr)
    , INIT_RESPONSE(rEcho)
    , INIT_ATTRIBUTE(attInt)
   {
      echo >> rEcho;
   }
};


INTERFACE(Simple2)
{
   Request<int> echo;
   Response<int> rEcho;
   
   Signal<double> sigUsr;
   
   Simple2()
    : INIT_REQUEST(echo)
    , INIT_SIGNAL(sigUsr)
    , INIT_RESPONSE(rEcho)
   {
      echo >> rEcho;
   }
};

}   // namespace


void sig_callback(double d)
{
    std::cout << "Having signal " << d << std::endl;
}


void att_callback(int i)
{
   std::cout << "Having attribute change " << i << std::endl;
}


void echo_callback(const spl::CallState&, int i)
{
   std::cout << "Reponse from server: " << i << std::endl;
}


int client()
{
   spl::Stub<test::Simple> sst("/my_simple", "org.simppl.simple_server");
   
   // FIXME client also possible without bus name?
   spl::Dispatcher disp("org.simppl.simple_client");
   disp.addClient(sst);
   
   sst.rEcho >> echo_callback;
   
   sst.sigUsr.attach() >> sig_callback;
   sst.echo(42);
   
   sst.attInt.attach() >> att_callback;
   
   return disp.run();
}


struct SimpleServer2 : spl::Skeleton<test::Simple2>
{
    SimpleServer2()
     : spl::Skeleton<test::Simple2>("/my_simple2")
    {
        echo >> std::bind(&SimpleServer2::handleEcho, this, _1);
    }
    
    void handleEcho(int i)
    {
        std::cout << "Client saying '" << i << "'" << std::endl;
        sigUsr.emit(3.1415);
        
        respondWith(rEcho(42));
    }
};


struct SimpleServer : spl::Skeleton<test::Simple>
{
    SimpleServer()
     : spl::Skeleton<test::Simple>("/my_simple")
    {
        echo >> std::bind(&SimpleServer::handleEcho, this, _1);
    }
    
    void handleEcho(int i)
    {
        std::cout << "Client saying '" << i << "'" << std::endl;
        sigUsr.emit(3.1415);
        
        respondWith(rEcho(42));
        SimpleServer2* s2 = new SimpleServer2();
        disp().addServer(*s2);
    }
};


int server()
{
   SimpleServer serv;
   
   spl::Dispatcher disp("org.simppl.simple_server");
   disp.addServer(serv);
   
   return disp.run();
}


int main(int argc, char** argv)
{
    return argc > 1 ? server() : client();
}
