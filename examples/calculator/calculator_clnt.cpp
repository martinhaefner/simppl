
#include "simppl/dispatcher.h"

#include "calculator.h"

#include <thread>


using namespace std::placeholders;

namespace spl = simppl::dbus;


std::atomic_bool gbl_connected(false);

static const char* rolename = "calculator";


struct CalculatorClient : spl::Stub<org::simppl::Calculator>
{
   CalculatorClient(const char* location)
    : spl::Stub<org::simppl::Calculator>(rolename, location)
   {
      connected >> std::bind(&CalculatorClient::handleConnected, this, _1);
   }

   void handleConnected(spl::ConnectionState st)
   {
       if (st == spl::ConnectionState::Connected)
       {
          gbl_connected.store(true);
          value.attach() >> std::bind(&CalculatorClient::valueChanged, this, _1, _2);
       }
       else
          gbl_connected.store(false);
   }

   void valueChanged(spl::CallState, double d)
   {
      std::cout << "Now having " << d << std::endl;
   }
};


int main(int argc, char** argv)
{
   if (argc > 1)
      rolename = argv[1];

    spl::Dispatcher disp("tcp:host=192.168.200.70,port=8877");

    CalculatorClient calc(rolename);
    disp.addClient(calc);

   std::thread t([&disp](){
       disp.run();
   });

   // wait for calculator to be ready
   while(!gbl_connected.load())
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

   char arg;
   double d;

   bool finished = false;
   while(!finished)
   {
      std::cin >> arg;

      if (gbl_connected.load())
      {
          switch(arg)
          {
             case '+':
                // FIXME make this inherent thread-safe if configured so
                std::cin >> d;
                calc.add(d);
                break;

             case '-':
                std::cin >> d;
                calc.sub(d);
                break;

             case 'c':
                std::cout << "Clear" << std::endl;
                calc.clear();
                break;

             case 'q':
                std::cout << "Quit" << std::endl;
                finished = true;
                break;

             default:
                std::cout << "Unknown command:" << std::endl;
                break;
          }
      }
      else
         finished = true;
   }

   disp.stop();
   t.join();

   return EXIT_SUCCESS;
}
