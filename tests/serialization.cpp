#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"
#include "simppl/string.h"

#include <thread>


using namespace std::literals::chrono_literals;

using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{


struct TestStruct
{
   int i;
   int j;
   
   std::string str;
};


INTERFACE(Serialization)
{
   Method<simppl::dbus::oneway> stop;

   Method<in<TestStruct>, out<TestStruct>> echo;
   
   inline
   Serialization()
    : INIT(stop)
    , INIT(echo)
   {
      // NOOP
   }
};

}   // namespace test


namespace simppl {
namespace dbus {


// user-provided codec
template<>
struct Codec<test::TestStruct>
{
   static 
   void encode(DBusMessageIter& iter, const test::TestStruct& s)
   {
      DBusMessageIter _iter;

      dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &_iter);
      
      // TODO move serialize out of 'detail' and rename to 'encode'
      detail::serialize(_iter, s.i, s.str, s.j);
      
      dbus_message_iter_close_container(&iter, &_iter);
   }
   
   
   static 
   void decode(DBusMessageIter& iter, test::TestStruct& s)
   {
      DBusMessageIter _iter;
      dbus_message_iter_recurse(&iter, &_iter);
      
      // TODO implement appropriate decode function
      simppl::dbus::Codec<int>::decode(_iter, s.i);
      simppl::dbus::Codec<std::string>::decode(_iter, s.str);
      simppl::dbus::Codec<int>::decode(_iter, s.j);
      
      // advance to next element
      dbus_message_iter_next(&iter);
   }
   
   
   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return os << "(isi)";
   }
};


}   // namespace dbus
}   // namespace simppl


using namespace test;


namespace {


struct Server : simppl::dbus::Skeleton<Serialization>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Serialization>(d, rolename)
   {
      stop >> [this]()
      {
         disp().stop();
      };


      echo >> [this](const TestStruct& s)
      {
         EXPECT_EQ(42, s.i);
         EXPECT_EQ(7, s.j);
         EXPECT_EQ(std::string("Hello World"), s.str);
         
         respond_with(echo(s));
      };
   }
};


}   // anonymous namespace


TEST(Serialization, user_codec)
{
   simppl::dbus::Dispatcher d("bus:session");
   
   std::thread t([](){
      simppl::dbus::Dispatcher d("bus:session");
      Server s(d, "ts");
      d.run();
   });

   simppl::dbus::Stub<Serialization> stub(d, "ts");

   // wait for server to get ready
   std::this_thread::sleep_for(100ms);

   TestStruct in{ 42, 7, "Hello World" };
   
   auto out = stub.echo(in);
   
   EXPECT_EQ(in.i, out.i);
   EXPECT_EQ(in.j, out.j);
   EXPECT_EQ(in.str, out.str);
   
   stub.stop();   // stop server
   t.join();
}