#include "simppl/dispatcher.h"
#include "simppl/skeleton.h"

#include "calculator.h"


using namespace std::placeholders;

namespace spl = simppl::ipc;


struct CalculatorImpl : spl::Skeleton<::Calculator>
{
   CalculatorImpl(const char* role)
    : spl::Skeleton<::Calculator>(role)
   {
      clear >> std::bind(&CalculatorImpl::handleClear, this);
      add >> std::bind(&CalculatorImpl::handleAdd, this, _1);
      sub >> std::bind(&CalculatorImpl::handleSub, this, _1);
   }
   
   void handleClear()
   {
      value = 0;
   }
   
   void handleAdd(double d)
   {
      double oldValue = value.value();
      value = oldValue + d;
   }
   
   void handleSub(double d)
   {
      double oldValue = value.value();
      value = oldValue - d;
   }
};


int main(int argc, char** argv)
{
   const char* role = "calculator";
   if (argc > 1)
      role = argv[1];
   
   spl::Dispatcher disp("unix:calculator");
   disp.enableBrokerage();

   CalculatorImpl calc(role);
   disp.addServer(calc);   
   
   return disp.run();
}
