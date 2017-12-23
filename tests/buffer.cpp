#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"
#include "simppl/buffer.h"

#include <thread>


using namespace std::literals::chrono_literals;

using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{


INTERFACE(Buffer)
{
   Method<simppl::dbus::oneway> stop;

   Method<in<simppl::dbus::FixedSizeBuffer<16>>, out<simppl::dbus::FixedSizeBuffer<16>>> echo;
   
   inline
   Buffer()
    : INIT(stop)
    , INIT(echo)
   {
      // NOOP
   }
};

}

using namespace test;


namespace {


struct X
{
   int32_t i;
   int32_t j;
   int32_t k;
   int32_t l;
};


struct Server : simppl::dbus::Skeleton<Buffer>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Buffer>(d, rolename)
   {
      stop >> [this]()
      {
         disp().stop();
      };


      echo >> [this](const simppl::dbus::FixedSizeBuffer<16>& buf)
      {
         // buffer is self-contained, no delete necessary here
         const X* x = (const X*)buf.ptr();
         
         EXPECT_EQ(1, x->i);
         EXPECT_EQ(2, x->j);
         EXPECT_EQ(3, x->k);
         EXPECT_EQ(4, x->l);
         
         respond_with(echo(buf));
      };
   }
};


}   // anonymous namespace


TEST(Buffer, echo)
{
   simppl::dbus::Dispatcher d("bus:session");
   
   std::thread t([](){
      simppl::dbus::Dispatcher d("bus:session");
      Server s(d, "buf");
      d.run();
   });

   simppl::dbus::Stub<Buffer> stub(d, "buf");

   // wait for server to get ready
   std::this_thread::sleep_for(100ms);

   X x;
   x.i = 1;
   x.j = 2;
   x.k = 3;
   x.l = 4;
   
   auto y = stub.echo(simppl::dbus::FixedSizeBuffer<sizeof(X)>(&x));
   
   EXPECT_EQ(0, ::memcmp(&x, y.ptr(), sizeof(x)));
   
   stub.stop();   // stop server
   t.join();
}
