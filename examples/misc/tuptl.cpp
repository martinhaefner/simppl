#include <iostream>
#include "tupletl.h"
#include "tie.h"


template<typename TupleT>
void hello(const TupleT& t)
{
    //t.template get<0>() = 1;
}


Tuple<int, double> func(int i, double d)
{
    return Tuple<int, double>(i+1, d+1);
}


struct Helper
{
   Helper()
    : i(42)
   {
      std::cout << "Helper()" << std::endl;
   }

   Helper(const Helper& rhs)
    : i(rhs.i)
   {
      std::cout << "Helper(const Helper&)" << std::endl;
   }

   ~Helper()
   {
      std::cout << "~Helper" << std::endl;
   }

   int i;
};


int main()
{
    TupleTL<TYPELIST_2(int, double)> t(3);
    double d = 6.34;
    
    int i;
    double d1;
    tie(i, d1) = func(42, 3.1415);
    std::cout << i << " " << d1 << std::endl;
    
    TupleTL<TYPELIST_4(int, double&, const short, float)> tup(3, d, 42, 123.123);
    std::cout << "size=" << sizeof(tup) << std::endl;
    std::cout << tup.get<0>() << std::endl;
    std::cout << tup.get<1>() << std::endl;
    std::cout << tup.get<2>() << std::endl;
    std::cout << tup.get<3>() << std::endl;    
    tup.get<1>() = 3.1415;
    
    //hello(tup);  // this should yield a compiler error
    std::cout << d << std::endl;

    TupleTL<TYPELIST_6(char, short, int, float, double, int)> tup1;
    tup1.get<5>() = 42;
    std::cout << "42 = " << tup1.get<5>() << std::endl;

   std::cout << "Reference type" << std::endl;
   {
    Helper h;
    Tuple<Helper&> tup2(h);
    std::cout << tup2.get<0>().i << std::endl;
    tup2.get<0>().i = 4711;
    std::cout << tup2.get<0>().i << std::endl;
   }

   std::cout << "Value type" << std::endl;
   {
    Helper h;
    Tuple<Helper> tup2(h);
    std::cout << tup2.get<0>().i << std::endl;
    tup2.get<0>().i = 4711;
    std::cout << tup2.get<0>().i << std::endl;
   }

   std::cout << "Pointer type" << std::endl;
   {
    Helper h;
    Helper h1;
    Tuple<Helper*> tup2(&h);
    std::cout << tup2.get<0>()->i << std::endl;
    tup2.get<0>()->i = 4711;
    tup2.get<0>() = &h1;
    std::cout << tup2.get<0>()->i << std::endl;

    const Tuple<Helper*>& tup3 = tup2;
    //tup3.get<0>()->i = 4711;   // FIXME should this be possible since the tuple itself stays const; and only allow this with a tuple of <const T*>?
    //tup3.get<0>() = &h1;
   }

    return 0;
}

