#include "include/tribool.h"

#include <iostream>


int main()
{
   tribool b1(false);
   tribool b2(indeterminate);

   if (b1)
   {
      std::cout << "true" << std::endl;
   }
   else if (!b1)
   {
      std::cout << "false" << std::endl;
   }
   else
   {
      std::cout << "indet" << std::endl;
   }
   
   std::cout << std::boolalpha << (b1 || b2) << std::endl;
   return 0;
}
