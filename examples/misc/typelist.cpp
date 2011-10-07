#include "typelist.h"


int main()
{   
   PopFront<PushFront<TYPELIST_3(int, double, float), char>::type>::type gaga;   
   
   PopBack<PushBack<TYPELIST_3(int, double, float), char>::type>::type gaga1;
   //int i = gaga1;
   
   return 0;
}
