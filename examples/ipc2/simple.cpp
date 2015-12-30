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
   
   Simple()
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


int client()
{
   spl::Stub<test::Simple> sst("/my_simple", "org.simppl.simple_server");
   
   // FIXME client also possible without bus name?
   spl::Dispatcher disp("org.simppl.simple_client");
   disp.addClient(sst);
   
   sst.sigUsr.attach() >> sig_callback;
   sst.echo(42);
   
   return disp.run();
}


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
