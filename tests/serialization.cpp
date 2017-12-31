#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"
#include "simppl/string.h"
#include "simppl/struct.h"

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


struct TestStruct2
{
   typedef typename 
         simppl::dbus::make_serializer<int, int, std::string>::type 
      serializer_type;
   
   int i;
   int j;
   
   std::string str;
};


#if SIMPPL_HAVE_BOOST_FUSION
struct TestStruct3
{
   int i;
   int j;
   
   std::string str;
};
#endif


INTERFACE(Serialization)
{
   Method<simppl::dbus::oneway> stop;

   Method<in<TestStruct>, out<TestStruct>> echo;
   Method<in<TestStruct2>, out<TestStruct2>> echo2;
   
#if SIMPPL_HAVE_BOOST_FUSION
   Method<in<TestStruct3>, out<TestStruct3>> echo3;
#endif

   inline
   Serialization()
    : INIT(stop)
    , INIT(echo)
    , INIT(echo2)
#if SIMPPL_HAVE_BOOST_FUSION
    , INIT(echo3)
#endif
   {
      // NOOP
   }
};

}   // namespace test


#if SIMPPL_HAVE_BOOST_FUSION
BOOST_FUSION_ADAPT_STRUCT(
   test::TestStruct3,
   (int, i)
   (int, j)
   (std::string, str)
)
#endif


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
      
      simppl::dbus::encode(_iter, s.i, s.str, s.j);
      
      dbus_message_iter_close_container(&iter, &_iter);
   }
   
   
   static 
   void decode(DBusMessageIter& iter, test::TestStruct& s)
   {
      DBusMessageIter _iter;
      dbus_message_iter_recurse(&iter, &_iter);
      
      simppl::dbus::decode(_iter, s.i, s.str, s.j);
      
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
      
      
      echo2 >> [this](const TestStruct2& s)
      {
         EXPECT_EQ(42, s.i);
         EXPECT_EQ(7, s.j);
         EXPECT_EQ(std::string("Hello World"), s.str);
         
         respond_with(echo2(s));
      };
      
#if SIMPPL_HAVE_BOOST_FUSION
      echo3 >> [this](const TestStruct3& s)
      {
         EXPECT_EQ(42, s.i);
         EXPECT_EQ(7, s.j);
         EXPECT_EQ(std::string("Hello World"), s.str);
         
         respond_with(echo3(s));
      };
#endif
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


TEST(Serialization, serializer_type)
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

   TestStruct2 in{ 42, 7, "Hello World" };
   
   auto out = stub.echo2(in);
   
   EXPECT_EQ(in.i, out.i);
   EXPECT_EQ(in.j, out.j);
   EXPECT_EQ(in.str, out.str);
   
   stub.stop();   // stop server
   t.join();
}


#if SIMPPL_HAVE_BOOST_FUSION
TEST(Serialization, fusion)
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

   TestStruct3 in{ 42, 7, "Hello World" };
   
   auto out = stub.echo3(in);
   
   EXPECT_EQ(in.i, out.i);
   EXPECT_EQ(in.j, out.j);
   EXPECT_EQ(in.str, out.str);
   
   stub.stop();   // stop server
   t.join();
}
#endif
