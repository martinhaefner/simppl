#include "include/bind.h"
#include "include/ref.h"

#include <iostream>
#include <stdio.h>
#include <memory>
#include <vector>
#include <algorithm>


// ----------------------------------------------------------------------------------


bool func(int i, double& d)
{
   std::cout << "func: " << i << " " << d << std::endl;
   d = 0;
   return true;
}


void func1()
{
   std::cout << "Hallo" << std::endl;
}


struct Hello
{
   int i_;

   Hello()
   {
       std::cout << "Hello constructor" << std::endl;
   }

   Hello(int i) : i_(i)
   {
       std::cout << "Hello constructor" << std::endl;
   }

   Hello(const Hello& h)
   {
       std::cout << "Hello COPY constructor" << std::endl;
   }

   void sayHello(int i) const
   {
      std::cout << "Hello World!" << i << std::endl;
   }
   
   void sayHelloN(int i) 
   {
      std::cout << "Hello World!" << i << std::endl;
   }

   void sayHello1()
   {
      std::cout << "Hello World!" << std::endl;
   }

   void sayHello3(int i, double& d)
   {
      std::cout << "Hello World!" << i << " " << d << std::endl;
      d = 3.1415;
   }

   void doIt()
   {
       std::cout << "i=" << i_ << std::endl;
   }
};


void f4(int i, double d, float f, char c)
{
   std::cout << "i=" << i << ", d=" << d << ", f=" << f << ", c=" << c << std::endl;
}


template<typename T>
void eval(T t)
{
   t();
}


template<typename T>
void typ(T t)
{
   typename T::functor_type x = 4;
}

bool fx(double& d)
{
   return true;
}


int printx(const char* format, int i)
{
   printf(format, i);
   return 0;
}


int main()
{
   Function<void(*)()> fu(func1);
   eval(fu);

   Function<bool(*)(int, double&)> f(func);
   double y = 3.1415;
   std::cout << f(42, y) << std::endl;

   Function<void(Hello::*)(int)const> f1(&Hello::sayHello);
   Hello h;
   const Hello* h1 = &h;
   f1(*h1, 42);

   // get rid of this template arguments if possible...
   eval(bind(&Hello::sayHello1, ref(h)));
   double x = 3.1415;
   eval(bind(func, 3, x));
   std::cout << x << std::endl;

   x = 6.666667;
   bind(func, 4, _1)(x);
   std::cout << x << std::endl;

   bind(&Hello::sayHello, ref(h), 23)();
   bind(&Hello::sayHello, ref(h), _1)(33);
   bind(&Hello::sayHello, _1, 24)(h);  // FIXME here no pointer possible
   bind(&Hello::sayHello, _1, _2)(h, 22);

   std::vector<int> iv;
   iv.push_back(2);
   iv.push_back(4);
   iv.push_back(3);
   iv.push_back(1);
   std::for_each(iv.begin(), iv.end(), bind(printx, "%d\n", _1));
   
   double d = 123.123;
   Hello h2(42);   
   
   bind(&Hello::sayHello3, _1, 42, _2)(h2, d);
   std::cout << "d=" << d << std::endl;

   std::vector<Hello> hv;
   hv.push_back(Hello());
   hv.push_back(Hello());
   std::for_each(hv.begin(), hv.end(), bind(&Hello::doIt, _1)); 
   
   Function<bool(*)(double& d)> bf;
   std::cout << !!bf << std::endl;
   bf = bind(func, 4, _1);
   std::cout << !!bf << std::endl;
   d = 7.777;
   bf(d);   
   
   Function<void(*)(Hello&, double&)> bf1(bind(&Hello::sayHello3, _1, 42, _2));
   bf1(h, d);
   
   bind(f4, _2, _3, _4, _1)(1, 2, 3, 4);   
   
   std::vector<Hello*> hv1;
   hv1.push_back(new Hello(42));
   std::for_each(hv1.begin(), hv1.end(), bind(&Hello::sayHello, _1, 23));
   
   bind(&Hello::sayHello, hv1.front(), _1)(23);
   
   // FIXME any smart pointer does not work here!
   std::auto_ptr<Hello> ah(new Hello(11));
   Function<void(Hello::*)(int, double&)> fu1(&Hello::sayHello3);
   fu1(*ah, 42, d);
   
   return 0;
}
