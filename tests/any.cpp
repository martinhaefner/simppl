#include <gtest/gtest.h>

#include <thread>
#include <chrono>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/interface.h"

#include "simppl/struct.h"
#include "simppl/string.h"
#include "simppl/vector.h"
#include "simppl/any.h"


using namespace std::literals::chrono_literals;

using simppl::dbus::in;
using simppl::dbus::out;
using simppl::dbus::oneway;


namespace test
{
   namespace any
   {
      struct complex
      {
          typedef typename simppl::dbus::make_serializer<double, double>::type serializer_type;

          complex() = default;
          complex(double re, double im)
           : re(re)
           , im(im)
          {
              // NOOP
          }

          double re;
          double im;
      };


      INTERFACE(AServer)
      {
         Method<in<simppl::dbus::Any>> set;
         Method<out<simppl::dbus::Any>> get;

         Method<in<simppl::dbus::Any>, out<std::vector<simppl::dbus::Any>>> complex;

         Method<in<int>, in<simppl::dbus::Any>, in<std::string>, out<int>, out<simppl::dbus::Any>, out<std::string>> in_the_middle;

         Method<oneway> stop;

         Method<out<simppl::dbus::Any>> getVecEmpty;
         Method<in<simppl::dbus::Any>, out<simppl::dbus::Any>> setGet;


         AServer()
          : INIT(set)
          , INIT(get)
          , INIT(complex)
          , INIT(in_the_middle)
          , INIT(stop)
          , INIT(getVecEmpty)
          , INIT(setGet)
         {
            // NOOP
         }
      };
   }
}


namespace {

   struct Client : simppl::dbus::Stub<test::any::AServer>
   {
      Client(simppl::dbus::Dispatcher& d)
       : simppl::dbus::Stub<test::any::AServer>(d, "role")
      {
         connected >> [this](simppl::dbus::ConnectionState s){

            get.async() >> [this](const simppl::dbus::CallState& state, const simppl::dbus::Any& a){

                EXPECT_EQ(42, a.as<int>());

                set.async(simppl::dbus::Any(42)) >> [this](const simppl::dbus::CallState& state){

                    std::vector<std::string> sv;
                    sv.push_back("Hello");
                    sv.push_back("World");

                    complex.async(sv) >> [this](const simppl::dbus::CallState& state, const std::vector<simppl::dbus::Any>& result){

                        EXPECT_EQ(3, result.size());
                        EXPECT_STREQ("Hello", result[0].as<std::string>().c_str());
                        EXPECT_EQ(42, result[1].as<int>());

                        // calling twice possible?
                        EXPECT_EQ(42, result[2].as<test::any::complex>().re);
                        EXPECT_EQ(4711, result[2].as<test::any::complex>().im);

                        disp().stop();
                    };
                };
            };
         };
      }
   };

   struct Server : simppl::dbus::Skeleton<test::any::AServer>
   {
      Server(simppl::dbus::Dispatcher& d)
       : simppl::dbus::Skeleton<test::any::AServer>(d, "role")
      {
         get >> [this](){

            simppl::dbus::Any a(42);

            respond_with(get(a));
         };


         set >> [this](const simppl::dbus::Any& a){

             EXPECT_EQ(true,  a.is<int>());
             EXPECT_EQ(false, a.is<double>());
             EXPECT_EQ(false, a.is<std::string>());

             EXPECT_EQ(42, a.as<int>());

             respond_with(set());
         };


         complex >> [this](const simppl::dbus::Any& a){

             auto sv = a.as<std::vector<std::string>>();

             EXPECT_EQ(2, sv.size());
             EXPECT_STREQ("Hello", sv[0].c_str());
             EXPECT_STREQ("World", sv[1].c_str());

             std::vector<simppl::dbus::Any> av;
             av.push_back(std::string("Hello"));
             av.push_back(int(42));
             av.push_back(test::any::complex(42,4711));

             respond_with(complex(av));
         };


         in_the_middle >> [this](int i, const simppl::dbus::Any& a, const std::string& str){

             // do never try to send a received any again,
             // create a new one from the received data
             simppl::dbus::Any ret = a.as<std::vector<int>>();

             respond_with(in_the_middle(i, ret, str));
         };


         stop >> [this](){

            this->disp().stop();
         };


         setGet >> [this](const simppl::dbus::Any& a){

            simppl::dbus::Any ret = a.as<std::vector<std::string>>();

            respond_with(setGet(ret));
         };


         getVecEmpty >> [this](){

            std::vector<std::string> vec;
            simppl::dbus::Any a(vec);

            respond_with(getVecEmpty(a));
         };
      }
   };
}


TEST(Any, method)
{
   simppl::dbus::Dispatcher d("bus:session");
   Client c(d);
   Server s(d);

   d.run();
}


TEST(Any, blocking_get)
{
    simppl::dbus::Dispatcher d("bus:session");

    std::thread t([](){
        simppl::dbus::Dispatcher d("bus:session");
        Server s(d);
        d.run();
    });

    simppl::dbus::Stub<test::any::AServer> stub(d, "role");

    // wait for server to get ready
    std::this_thread::sleep_for(200ms);

    simppl::dbus::Any a = stub.get();
    EXPECT_EQ(42, a.as<int>());

    stub.stop();   // stop server
    t.join();
}


TEST(Any, blocking_set)
{
    simppl::dbus::Dispatcher d("bus:session");

    std::thread t([](){
        simppl::dbus::Dispatcher d("bus:session");
        Server s(d);
        d.run();
    });

    simppl::dbus::Stub<test::any::AServer> stub(d, "role");

    // wait for server to get ready
    std::this_thread::sleep_for(200ms);

    stub.set(42);

    stub.stop();   // stop server
    t.join();
}


TEST(Any, blocking_complex)
{
    simppl::dbus::Dispatcher d("bus:session");

    std::thread t([](){
        simppl::dbus::Dispatcher d("bus:session");
        Server s(d);
        d.run();
    });

    simppl::dbus::Stub<test::any::AServer> stub(d, "role");

    // wait for server to get ready
    std::this_thread::sleep_for(200ms);

    std::vector<std::string> sv;
    sv.push_back("Hello");
    sv.push_back("World");

    auto result = stub.complex(sv);

    EXPECT_EQ(3, result.size());
    EXPECT_STREQ("Hello", result[0].as<std::string>().c_str());
    EXPECT_EQ(42, result[1].as<int>());

    // calling twice possible?
    EXPECT_EQ(42, result[2].as<test::any::complex>().re);
    EXPECT_EQ(4711, result[2].as<test::any::complex>().im);

    stub.stop();   // stop server
    t.join();
}


TEST(Any, blocking_in_the_middle)
{
    simppl::dbus::Dispatcher d("bus:session");

    std::thread t([](){
        simppl::dbus::Dispatcher d("bus:session");
        Server s(d);
        d.run();
    });

    simppl::dbus::Stub<test::any::AServer> stub(d, "role");

    // wait for server to get ready
    std::this_thread::sleep_for(200ms);

    std::vector<int> iv;
    iv.push_back(7777);
    iv.push_back(4711);

    int i;
    simppl::dbus::Any result;
    std::string str;

    try
    {
        i=21;
        std::tie(i, result, str) = stub.in_the_middle(42, iv, "Hallo Welt");
    }
    catch(std::exception& ex)
    {
        EXPECT_TRUE(false);
    }

    EXPECT_EQ(42, i);

    // calling as<...> multiple times is ok?
    EXPECT_EQ(2, result.as<std::vector<int>>().size());
    EXPECT_EQ(7777, result.as<std::vector<int>>()[0]);
    EXPECT_EQ(4711, result.as<std::vector<int>>()[1]);

    EXPECT_STREQ("Hallo Welt", str.c_str());

    stub.stop();   // stop server
    t.join();
}


TEST(Any, types)
{
    simppl::dbus::Any a(42);
    EXPECT_TRUE(a.is<int>());
    EXPECT_FALSE(a.is<double>());
    EXPECT_FALSE(a.is<std::string>());
    EXPECT_EQ(42, a.as<int>());

    simppl::dbus::Any b(42.42);
    EXPECT_TRUE(b.is<double>());
    EXPECT_FALSE(b.is<int>());
    EXPECT_FALSE(b.is<std::string>());
    EXPECT_EQ(42.42, b.as<double>());

    simppl::dbus::Any c(std::string("Hello"));
    EXPECT_FALSE(c.is<double>());
    EXPECT_FALSE(c.is<int>());
    EXPECT_TRUE(c.is<std::string>());
    EXPECT_STREQ("Hello", c.as<std::string>().c_str());

    simppl::dbus::Any d;
    EXPECT_FALSE(d.is<int>());
    EXPECT_FALSE(d.is<double>());
    EXPECT_FALSE(d.is<std::string>());
    EXPECT_FALSE(d.is<std::vector<int>>());

    simppl::dbus::Any e(std::vector<int>{ 1, 2, 3 });
    EXPECT_FALSE(e.is<int>());
    EXPECT_FALSE(e.is<double>());
    EXPECT_FALSE(e.is<std::string>());
    EXPECT_TRUE(e.is<std::vector<int>>());

    auto v = e.as<std::vector<int>>();
    EXPECT_EQ(3, v.size());
    EXPECT_EQ(1, v[0]);
    EXPECT_EQ(2, v[1]);
    EXPECT_EQ(3, v[2]);
}


TEST(Any, empty)
{
    simppl::dbus::Dispatcher d("bus:session");

    std::thread t([](){
        simppl::dbus::Dispatcher d("bus:session");
        Server s(d);
        d.run();
    });

    simppl::dbus::Stub<test::any::AServer> stub(d, "role");

    // wait for server to get ready
    std::this_thread::sleep_for(200ms);

    simppl::dbus::Any a = stub.getVecEmpty();

    EXPECT_TRUE(a.is<std::vector<std::string>>());

    for (size_t i = 0; i < 10; i++)
    {
        auto arg = a.as<std::vector<std::string>>();
        a = stub.setGet(arg);

        EXPECT_TRUE(a.is<std::vector<std::string>>());
    }

    stub.stop();   // stop server
    t.join();
}
