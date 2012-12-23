#include <iostream>

#include "simppl/variant.h"

using namespace simppl;


struct MyVisitor : StaticVisitor<>
{
   inline
   void operator()(int i)
   {
      std::cout << "is int: " << i << std::endl;
   }

   inline
   void operator()(const std::string& s)
   {
      std::cout << "is string: " << s << std::endl;
   }

   inline
   void operator()(double d)
   {
      std::cout << "is double: " << d << std::endl;
   }
};


struct CheckVisitor : StaticVisitor<bool>
{
   inline
   bool operator()(int i)
   {
      return i > 0;
   }

   inline
   bool operator()(const std::string& s)
   {
      return s.size() > 0;
   }

   inline
   bool operator()(double d)
   {
      return d > 0;
   }
};


struct First
{
   int i;
    void sayHello() const 
    {
        std::cout << "Say Hello" << std::endl;
    }
};

struct Second
{
   int i;
    void saySomething() const 
    {
        std::cout << "Say Something" << std::endl;
    }
};


struct StrategyVisitor : StaticVisitor<>
{
    inline
    void operator()(const First& f)
    {     
        f.sayHello();
    }

    inline
    void operator()(const Second& s)
    {      
        s.saySomething();
    }
};


int main()
{         
   Variant<First, Second> the_switch;   
   the_switch = First();
   //the_switch.get<First>()->sayHello();
   staticVisit(StrategyVisitor(), the_switch);

   Variant<int, std::string, double> v(std::string("Hallo Welt"));
   std::cout << "Sizeof = " << sizeof(v) << std::endl;
   std::cout << v.get<int>() << std::endl;             // must be 0
   std::cout << *v.get<std::string>() << std::endl;    // must be "Hallo Welt"
   staticVisit(MyVisitor(), v);

   v = 42;
   std::cout << *v.get<int>() << std::endl;       // must be 42
   std::cout << v.get<double>() << std::endl;     // must be 0
   staticVisit(MyVisitor(), v);

   ++(*v.get<int>());
   std::cout << "Now 43: " << *v.get<int>() << std::endl;       // must be 43

   v = -1;
   std::cout << "is > 0: " << std::boolalpha << staticVisit(CheckVisitor(), v) << std::endl;

   v = 1;   // do not destruct, use assignment operator here
   std::cout << "is > 0: " << std::boolalpha << staticVisit(CheckVisitor(), v) << std::endl;

   v = std::string("Hallo");
   std::cout << "is > 0: " << std::boolalpha << staticVisit(CheckVisitor(), v) << std::endl;

   return 0;
}
