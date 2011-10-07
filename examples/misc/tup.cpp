#include <iostream>

#include "include/tuple.h"
#include "include/tie.h"


template<typename TupleT>
void hello(const TupleT& t)
{
    t.template get<0>() = 1;
}


Tuple<int, double> func(int i, double d)
{
    return Tuple<int, double>(i+1, d+1);
}


int main()
{
    Tuple<int, double> t(3);
    double d = 6.34;
    
    int i;
    double d1;
    tie(i, d1) = func(42, 3.1415);
    std::cout << i << " " << d1 << std::endl;
    
    Tuple<int, double&, const short, float> tup(3, d, 42, 123.123);
    std::cout << "size=" << sizeof(tup) << std::endl;
    std::cout << tup.get<0>() << std::endl;
    std::cout << tup.get<1>() << std::endl;
    std::cout << tup.get<2>() << std::endl;
    std::cout << tup.get<3>() << std::endl;    
    tup.get<1>() = 3.1415;
    
    //hello(tup);  // this should yield a compiler error
    std::cout << d << std::endl;

    return 0;
}

