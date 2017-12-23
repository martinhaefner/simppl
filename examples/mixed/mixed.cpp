/**
 * This example is not really good pratice. It shows that mixing blocking
 * function calls and event loop driven code is possible without 
 * loosing events in the meantime of the blocking situation. So after 
 * starting the program the output freezes after 4 ticks in order
 * to wait for the long lasting blocking function call and continues
 * with all following ticks so no gap will occur. 
 */
#include <thread>
#include <chrono>
#include <iostream>

#include "simppl/interface.h"

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/string.h"


namespace test
{
   using simppl::dbus::in;
   using simppl::dbus::out;
   

   INTERFACE(clock)
   {
      Signal<int> tick;
            
      clock()
       : INIT(tick)
      {
         // NOOP
      }
   };
   
   
   INTERFACE(long_op)
   {
      Method<in<std::string>, out<std::string>> do_it;
      
      long_op()
       : INIT(do_it)
      {
         // NOOP
      }
   };
}


void the_client()
{
   simppl::dbus::Dispatcher d;
   simppl::dbus::Stub<test::clock> clock(d, "clock");
   simppl::dbus::Stub<test::long_op> op(d, "op");
   
   clock.connected = [&clock, &op](simppl::dbus::ConnectionState st){
      clock.tick.attach() >> [&op](int count){
         std::cout << "tick: " << count << std::endl;
         
         if (count == 4)
         {
            // mixing blocking and non-blocking calls is not really a good
            // idea, but this is for demonstration that it works only
            std::string rc = op.do_it("Hello");
            std::cout << "Response: " << rc << std::endl;
         }
      };
   };
   
   d.run();
}


void the_clock()
{
   simppl::dbus::Dispatcher d;
   simppl::dbus::Skeleton<test::clock> skel(d, "clock");
   
   std::thread t([&skel](){
      int i=0;
      
      while(true)
      {
         skel.tick.notify(i++);
         std::this_thread::sleep_for(std::chrono::seconds(1));
      }
   });
   
   d.run();
}


void the_long_op()
{
   simppl::dbus::Dispatcher d;
   simppl::dbus::Skeleton<test::long_op> skel(d, "op");
   
   // just echo with some waiting time
   skel.do_it >> [&skel](const std::string& str){
      std::this_thread::sleep_for(std::chrono::seconds(5));    
      skel.respond_with(skel.do_it(str));
   };
   
   d.run();
}


int main()
{
   simppl::dbus::enable_threads();
   
   std::thread t1(the_long_op);
   std::thread t2(the_clock);
   
   std::thread t3(the_client);
   
   while(true)
      std::this_thread::sleep_for(std::chrono::seconds(1));
      
   return 0;
}


