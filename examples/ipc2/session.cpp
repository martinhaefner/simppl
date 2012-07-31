#include "include/ipc2.h"


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


struct Server : Skeleton<BankAccount>
{
   Server(const char* role)
    : Skeleton<BankAccount>(role)
   {  
      login >> std::tr1::bind(&Server::handleLogin, this, _1, _2);
      getDeposit >> std::tr1::bind(&Server::handleGetDeposit, this);
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
         respondWith(RuntimeError(-1, "login invalid"));
   }
};


struct Client : Stub<BankAccount>
{
   Client(const char* user, const char* role)
    : Stub<BankAccount>(role, "unix:myserver")   // connect the client to 'myserver'
    , user_(user)
   {
      resultOfGetDeposit >> std::tr1::bind(&Client::handleGetDeposit, this, _1);
   }
   
   void connected()
   {
      login(user_, "passwd");
      getDeposit();
   }
   
   void handleGetDeposit(int value)
   {
      std::cout << "Deposit is " << value << std::endl;
   }

   std::string user_;
};


void* server(void* dispatcher)
{
   Dispatcher& d = *(Dispatcher*)dispatcher;
   
   Server s1("myrole");
   d.addServer(s1);
   
   d.run();
   return 0;
}


int main()
{
   // start server dispatcher thread on unix path 'myserver'
   Dispatcher server_dispatcher("unix:myserver");
   
   pthread_t tid;
   pthread_create(&tid, 0, server, &server_dispatcher);
   while(!server_dispatcher.isRunning());   // wait for other thread
   
   // run client in separate thread (not really necessary, just for blocking interfaces)
   Dispatcher d;
   
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
