#include "simppl/dispatcher.h"
#include "simppl/skeleton.h"

#include "calculator.h"


using namespace std::placeholders;

namespace spl = simppl::dbus;


struct CalculatorImpl : spl::Skeleton<org::simppl::Calculator>
{
   CalculatorImpl(const char* role)
    : spl::Skeleton<org::simppl::Calculator>(role)
   {
       value = 0;

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

   spl::Dispatcher disp("dbus:session");

   CalculatorImpl calc(role);
   disp.addServer(calc);

   return disp.run();
}
