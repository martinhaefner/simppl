//
// example of self-made broker interface handling
//

#include "calculator.h"

#include <pthread.h>


struct CalculatorClient;
static CalculatorClient* calc = 0;

static const char* rolename = "calculator";
   

struct CalculatorClient : Stub<Calculator>
{
   CalculatorClient(const char* location)
    : Stub<Calculator>(rolename, location)
   {
      connected >> std::bind(&CalculatorClient::handleConnected, this);
   }
   
   void handleConnected()
   {
      value.attach() >> std::bind(&CalculatorClient::valueChanged, this, _1);
   }
   
   void valueChanged(double d)
   {
      std::cout << "Now having " << d << std::endl;
   }
};


void ClientsFactory(Dispatcher* disp, const std::string& fullName, const std::string& location)
{
   if (fullName == fullQualifiedName("Calculator", rolename))
   {
      CalculatorClient* new_calc = new CalculatorClient(location.c_str());
      disp->addClient(*new_calc);
      
      calc = new_calc;
   }
}


void* threadRunner(void* arg)
{
   Dispatcher disp;

   BrokerClient broker(disp);
   broker.waitForService(fullQualifiedName("Calculator", rolename), std::bind(ClientsFactory, &disp, _1, _2));
   
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
            std::cout << "Unknown command:" << std::endl;
            break;
      }
   }   
}
