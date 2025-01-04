#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/interface.h"
#include "simppl/struct.h"

#include <thread>


using namespace std::literals::chrono_literals;

using simppl::dbus::in;
using simppl::dbus::out;
using simppl::dbus::_throw;


namespace test
{

/**
 * Due to the virtual function table provided by the base std::exception
 * is it not possible to use the make_serializer here.
 *
 * But with boost::fusion everything works ok.
 *
 * @note you must make sure that the dbus message must contain
 * data in addition to the error name, otherwise the exception type returned
 * by the client side API will just be simppl::dbus::Error.
 */
struct MyException : simppl::dbus::Error
{
    /// class needs a default constructor which is used during deserialization.
    /// If such an exception instance is returned on the server, the name
    /// will be set via C++ RTTI.
    MyException() = default;

    /// This is the instance that can be thrown in user code.
    MyException(int return_code)
     : simppl::dbus::Error("My.Exception")
     , rc(return_code)
    {
        // NOOP
    }

    int rc;
};


INTERFACE(Errors)
{
   Method<simppl::dbus::oneway> stop;

   Method<> hello;
   Method<in<int>, out<int>> hello1;

#if SIMPPL_HAVE_BOOST_FUSION
   Method<_throw<MyException>> hello2;
   Method<_throw<MyException>> hello3;
   Method<_throw<MyException>> hello4;
#endif

   inline
   Errors()
    : INIT(stop)
    , INIT(hello)
    , INIT(hello1)
#if SIMPPL_HAVE_BOOST_FUSION
    , INIT(hello2)
    , INIT(hello3)
    , INIT(hello4)
#endif
   {
      // NOOP
   }
};

}


#if SIMPPL_HAVE_BOOST_FUSION
// must be outside of namespace
BOOST_FUSION_ADAPT_STRUCT(
   test::MyException,
   (int, rc)
)
#endif


using namespace test;


namespace {


struct Client : simppl::dbus::Stub<Errors>
{
   Client(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Errors>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         hello.async() >> [this](const simppl::dbus::CallState& state){
            EXPECT_FALSE((bool)state);
            EXPECT_STREQ(state.exception().name(), "shit.happens");

#if SIMPPL_HAVE_BOOST_FUSION
            hello2.async() >> [this](const simppl::dbus::CallState& state){
                EXPECT_FALSE((bool)state);
                EXPECT_STREQ(state.exception().name(), "My.Exception");
                EXPECT_EQ(42, state.as<MyException>().rc);

                hello3.async() >> [this](const simppl::dbus::CallState& state){
                    EXPECT_FALSE((bool)state);
                    EXPECT_STREQ(state.exception().name(), "simppl.dbus.UnhandledException");
#endif
                    hello1.async(42) >> [this](const simppl::dbus::CallState& state, int){
                        EXPECT_FALSE((bool)state);
                        EXPECT_STREQ(state.exception().name(), "also.shit");

                        disp().stop();
                    };
#if SIMPPL_HAVE_BOOST_FUSION
                };
            };
#endif
         };
      };
   }
};


struct Server : simppl::dbus::Skeleton<Errors>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Errors>(d, rolename)
   {
      stop >> [this](){
         disp().stop();
      };

      hello >> [this](){
         respond_with(simppl::dbus::Error("shit.happens"));
      };

      hello1 >> [this](int){
         respond_with(simppl::dbus::Error("also.shit"));
      };

#if SIMPPL_HAVE_BOOST_FUSION
      hello2 >> [this](){
         respond_with(MyException(42));
      };

      hello3 >> [this](){
         throw std::runtime_error("Ooops");
      };

      hello4 >> [this](){
         respond_with(MyException());   // will produce an DBus error test.MyException
      };
#endif
   }
};


}   // anonymous namespace


TEST(Errors, default_type)
{
	simppl::dbus::Dispatcher d("bus:session");
    Client c(d);
    
    bool called = false;
    
    try
    {
		c.hello2();
	}
	catch(const test::MyException& e)
    {
		ASSERT_FALSE(true);        
    }
    catch(const simppl::dbus::Error& e)
    {
		// since server is not running, the returned error must be of simppl standard error type
		called = true;		
    }
    EXPECT_TRUE(called);
    
    c.hello2.async() >> [](const simppl::dbus::CallState& state){
				
		EXPECT_STREQ(state.exception().name(), "My.Exception");
		
		bool called = false;
		try
		{
			state.as<MyException>();
			EXPECT_TRUE(false);
		}
		catch(simppl::dbus::Error&)
		{
			EXPECT_TRUE(false);
		}
		catch(std::runtime_error&)
		{
			called = true;
		}
		EXPECT_TRUE(called);
	};
}


TEST(Errors, methods)
{
   simppl::dbus::Dispatcher d("bus:session");
   Client c(d);
   Server s(d, "s");

   d.run();
}


static void blockrunner()
{
   simppl::dbus::Dispatcher d("bus:session");
   Server s(d, "s");

   d.run();
}


TEST(Errors, blocking)
{
   std::thread t(blockrunner);

   // wait for server to get ready
   std::this_thread::sleep_for(200ms);

   simppl::dbus::Dispatcher d("bus:session");

   simppl::dbus::Stub<Errors> stub(d, "s");

   try
   {
      stub.hello();

      // never reach
      ASSERT_FALSE(true);
   }
   catch(const simppl::dbus::Error& e)
   {
      EXPECT_STREQ(e.name(), "shit.happens");
   }
   catch(...)
   {
      ASSERT_FALSE(true);
   }

   try
   {
      int res = stub.hello1(101);
      (void)res; //remove unused warning
      // never reach
      ASSERT_FALSE(true);
   }
   catch(const simppl::dbus::Error& e)
   {
      EXPECT_STREQ(e.name(), "also.shit");
   }
   catch(...)
   {
      ASSERT_FALSE(true);
   }

#if SIMPPL_HAVE_BOOST_FUSION
   try
   {
      stub.hello2();

      // never reach
      ASSERT_FALSE(true);
   }
   catch(const test::MyException& e)
   {
       EXPECT_STREQ(e.name(), "My.Exception");
       EXPECT_EQ(42, e.rc);
   }
   catch(const simppl::dbus::Error& e)
   {
       // does not reach the default exception handler
       ASSERT_FALSE(true);
   }
   catch(...)
   {
      ASSERT_FALSE(true);
   }

   try
   {
      stub.hello3();

      // never reach
      ASSERT_FALSE(true);
   }
   catch(const test::MyException& e)
   {
	   ASSERT_FALSE(true);       
   }
   catch(const simppl::dbus::Error& e)
   {
       // unhandled exception will be mapped to default error
       ASSERT_TRUE(true);
   }
   catch(...)
   {
      ASSERT_FALSE(true);
   }

   try
   {
      stub.hello4();

      // never reach
      ASSERT_FALSE(true);
   }
   catch(const test::MyException& e)
   {
       EXPECT_STREQ(e.name(), "test.MyException");
       EXPECT_EQ(0, e.rc);
   }
   catch(const simppl::dbus::RuntimeError& e)
   {
       ASSERT_FALSE(true);
   }
   catch(const simppl::dbus::Error& e)
   {
       // does not reach the default exception handler
       ASSERT_FALSE(true);
   }
   catch(...)
   {
       ASSERT_FALSE(true);
   }
#endif

   stub.stop();
   t.join();
}
