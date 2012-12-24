//
// example of integrated broker interface handling
//

#include "simppl/dispatcher.h"

#include "calculator.h"

#include <pthread.h>


using namespace std::placeholders;

namespace spl = simppl::ipc;


static const char* rolename = "calculator";
   
struct CalculatorClient;
static CalculatorClient* calc = 0;


struct CalculatorClient : spl::Stub<Calculator>
{
   CalculatorClient()
    : spl::Stub<Calculator>(rolename, "auto:")
   {
      connected >> std::bind(&CalculatorClient::handleConnected, this);
   }
   
   void handleConnected()
   {
      value.attach() >> std::bind(&CalculatorClient::valueChanged, this, _1);
      calc = this;
   }
   
   void valueChanged(double d)
   {
      std::cout << "Now having " << d << std::endl;
   }
};


void* threadRunner(void* arg)
{
   spl::Dispatcher disp;
   disp.enableBrokerage();
   
   CalculatorClient clnt;
   disp.addClient(clnt);
   
   disp.run();
   return 0;
}


int main(int argc, char** argv)
{
   if (argc > 1)
      rolename = argv[1];
   
   pthread_t thread;
   pthread_create(&thread, 0, threadRunner, 0);
   
   // wait for calculator to be ready
   while(!calc)
      usleep(100);
   
   char arg;
   double d;
   
   bool finished = false;
   while(!finished)
   {
      std::cout << "Input: q(uit),c(lear),+-<value> > ";
      std::cin >> arg;
      switch(arg)
      {
         case '+':
            // FIXME make this inherent thread-safe if configured so
            std::cin >> d;
            calc->add(d);
            break;
            
         case '-':
            std::cin >> d;
            calc->sub(d);
            break;
         
         case 'c':
            std::cout << "Clear" << std::endl;
            calc->clear();
            break;
         
         case 'q':
            std::cout << "Quit" << std::endl;
            finished = true;
            break;
            
         default:
            std::cout << "Unknown command: " << arg << std::endl;
            break;
      }
   }   
}
