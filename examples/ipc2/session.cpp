#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <thread>


using namespace std::placeholders;

namespace spl = simppl::ipc;


struct SessionSpec
{
   /// destruction helper function
   static void destruct(void* ptr)
   {
      delete (SessionSpec*)ptr;
   }
   
   int returnvalue_;   ///< the account information
};


INTERFACE(BankAccount)
{   
   Request<std::string, std::string> login;
   
   Request<> getDeposit;
   Response<int> resultOfGetDeposit;
   
   inline
   BankAccount()
    : INIT_REQUEST(login)
    , INIT_REQUEST(getDeposit)
    , INIT_RESPONSE(resultOfGetDeposit)
   {
      getDeposit >> resultOfGetDeposit;
   }
};


struct Server : spl::Skeleton<::BankAccount>
{
   Server(const char* role)
    : spl::Skeleton<::BankAccount>(role)
   {  
      login >> std::bind(&Server::handleLogin, this, _1, _2);
      getDeposit >> std::bind(&Server::handleGetDeposit, this);
   }
   
   
   void handleLogin(const std::string& user, const std::string& pass)
   {
      std::cout << "Login as " << user << "/" << pass << std::endl;
      
      SessionSpec* session = (SessionSpec*)currentSessionData();
      if (!session)
      {
         if (user == "hinz")
         {
            session = new SessionSpec();
            registerSession(session, SessionSpec::destruct);
         
            session->returnvalue_ = 42;
         }
         else if (user == "kunz")
         {
            session = new SessionSpec();
            registerSession(session, SessionSpec::destruct);
         
            session->returnvalue_ = 21;
         }
      }
   }
   
   void handleGetDeposit()
   {
      std::cout << "handleGetDeposit" << std::endl;
      
      SessionSpec* session = (SessionSpec*)currentSessionData();
      
      if (session)
      {
         respondWith(resultOfGetDeposit(session->returnvalue_));
      }
      else
         respondWith(spl::RuntimeError(-1, "invalid session"));
   }
};


struct Client : spl::Stub<::BankAccount>
{
   Client(const char* user, const char* role)
    : spl::Stub<::BankAccount>(role, "unix:myserver")   // connect the client to 'myserver'
    , user_(user)
   {
      connected >> std::bind(&Client::handleConnected, this, _1);
      resultOfGetDeposit >> std::bind(&Client::handleGetDeposit, this, _1, _2);
   }
   
   void handleConnected(simppl::ipc::ConnectionState)
   {
      login(user_, "passwd");
      getDeposit();
   }
   
   void handleGetDeposit(const spl::CallState& state, int value)
   {
      if (state)
      {
         std::cout << "Deposit is " << value << std::endl;
      }
      else
         std::cout << "Error returning deposit of user '" << user_ << "': " << state.what() << std::endl;
   }

   std::string user_;
};


int main()
{
   // start server dispatcher thread on unix path 'myserver'
   spl::Dispatcher server_dispatcher("unix:myserver");
   
   std::thread server_thread([&server_dispatcher](){
      Server s1("myrole");
      server_dispatcher.addServer(s1);
      server_dispatcher.run();
   });

   while(!server_dispatcher.isRunning());   // wait for other thread
   
   // run client in separate thread (not really necessary, just for blocking interfaces)
   spl::Dispatcher d;
   
   Client c1("hinz", "myrole");
   d.addClient(c1);
   
   Client c2("kunz", "myrole");
   d.addClient(c2);
   
   Client c3("gaga", "myrole");
   d.addClient(c3);
   
   d.run();
   
   server_dispatcher.stop();
   server_thread.join();
   
   return 0;

}
