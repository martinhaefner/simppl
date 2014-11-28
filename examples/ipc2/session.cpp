#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

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
         respondWith(spl::RuntimeError(-1, "login invalid"));
   }
};


struct Client : spl::Stub<::BankAccount>
{
   Client(const char* user, const char* role)
    : spl::Stub<::BankAccount>(role, "unix:myserver")   // connect the client to 'myserver'
    , user_(user)
   {
      connected >> std::bind(&Client::handleConnected, this);
      resultOfGetDeposit >> std::bind(&Client::handleGetDeposit, this, _1, _2);
   }
   
   void handleConnected()
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


void* server(void* dispatcher)
{
   spl::Dispatcher& d = *(spl::Dispatcher*)dispatcher;
   
   Server s1("myrole");
   d.addServer(s1);
   
   d.run();
   return 0;
}


int main()
{
   // start server dispatcher thread on unix path 'myserver'
   spl::Dispatcher server_dispatcher("unix:myserver");
   
   pthread_t tid;
   pthread_create(&tid, 0, server, &server_dispatcher);
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
   pthread_join(tid, 0);
   
   return 0;

}
