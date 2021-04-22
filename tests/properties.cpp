#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"
#include "simppl/objectpath.h"
#include "simppl/map.h"
#include "simppl/string.h"

#include <thread>


using namespace std::literals::chrono_literals;

using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{

enum ident_t {
   One=1, Two, Three, Four, Five, Six
};

struct gaga
{
};

INTERFACE(Properties)
{
   Method<in<int>, in<std::string>> set;
   Method<simppl::dbus::oneway> shutdown;

   Property<int, simppl::dbus::ReadWrite|simppl::dbus::Notifying> data;
   Property<std::map<ident_t, std::string>> props;

   Signal<int> mayShutdown;

   inline
   Properties()
    : INIT(set)
    , INIT(shutdown)
    , INIT(data)
    , INIT(props)
    , INIT(mayShutdown)
   {
      // NOOP
   }
};

}

using namespace test;


namespace {


struct Client : simppl::dbus::Stub<Properties>
{
   Client(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Properties>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         props.attach() >> [this](const simppl::dbus::CallState&, const std::map<ident_t, std::string>& props){
            ++callback_count_;

            if (callback_count_ == 1)   // the property Get(...) from the attach
            {
               EXPECT_EQ(2u, props.size());

               EXPECT_TRUE(props.find(Four) == props.end());
            }
            else if (callback_count_ == 2)   // the response on the assignment within the set(...) request
            {
               EXPECT_EQ(3u, props.size());

               EXPECT_TRUE(props.find(Four) != props.end());

               // no more signals on this client
               this->props.detach();

               mayShutdown.attach() >> [this](int){
                  if (++count_ == 2)
                  shutdown();
               };

               // one more roundtrip to see if further signals arrive...
               set.async(Five, "Five");
               set.async(Six, "Six");
            }
         };

         set.async(Four, "Four");
      };
   }

   int callback_count_ = 0;
   int count_ = 0;
};


struct GetterClient : simppl::dbus::Stub<Properties>
{
   GetterClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Properties>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         // never use operator= here, it's blocking!
         data.get_async() >> [this](const simppl::dbus::CallState& cs, int i){
             EXPECT_TRUE((bool)cs);
             EXPECT_EQ(i, 4711);

             disp().stop();
         };
      };
   }
};


struct GetterErrorClient : simppl::dbus::Stub<Properties>
{
   GetterErrorClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Properties>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         // never use operator= here, it's blocking!
         data.get_async() >> [this](const simppl::dbus::CallState& cs, int i){
             EXPECT_FALSE((bool)cs);
             EXPECT_STREQ(cs.exception().what(), "Not.Available");

             disp().stop();
         };
      };
   }
};


struct MultiClient : simppl::dbus::Stub<Properties>
{
   MultiClient(simppl::dbus::Dispatcher& d, bool attach)
    : simppl::dbus::Stub<Properties>(d, "s")
    , attach_(attach)
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         if (attach_)
         {
            props.attach() >> [this](const simppl::dbus::CallState&, const std::map<ident_t, std::string>& props){
               ++callback_count_;

               if (callback_count_ == 1)
               {
                  EXPECT_EQ(2u, props.size());

                  EXPECT_TRUE(props.find(Four) == props.end());
               }
               else if (callback_count_ == 2)
               {
                  EXPECT_EQ(3u, props.size());

                  EXPECT_TRUE(props.find(Four) != props.end());

                  shutdown();
               }
            };

            set.async(Four, "Four");
         }
      };
   }

   bool attach_;
   int callback_count_ = 0;
};


struct SetterClient : simppl::dbus::Stub<Properties>
{
   SetterClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Properties>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         data.attach() >> [this](const simppl::dbus::CallState&, int i){
            ++callback_count_;

            if (callback_count_ == 1)   // the property Get(...) from the attach
            {
               EXPECT_EQ(4711, i);
            }
            else if (callback_count_ == 2)   // the response on the assignment
            {
               EXPECT_EQ(5555, i);
            }
         };

         // never use operator= here, it's blocking!
         data.set_async(5555) >> [this](const simppl::dbus::CallState& cs){
             EXPECT_TRUE((bool)cs);

             disp().stop();
         };
      };
   }

   int callback_count_ = 0;
};


struct InvalidSetterClient : simppl::dbus::Stub<Properties>
{
   InvalidSetterClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Properties>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         data.attach() >> [this](const simppl::dbus::CallState&, int) {
             ++callback_count_;
         };

         // never use operator= here, it's blocking!
         data.set_async(-1) >> [this](const simppl::dbus::CallState& cs){
             EXPECT_FALSE((bool)cs);
             EXPECT_STREQ(cs.exception().what(), "simppl.dbus.UnhandledException");
         };

         data.set_async(1) >> [this](const simppl::dbus::CallState& cs){
             EXPECT_FALSE((bool)cs);
             EXPECT_STREQ(cs.exception().what(), "Invalid.Argument");
         };

         data.set_async(2) >> [this](const simppl::dbus::CallState& cs){
             EXPECT_TRUE((bool)cs);
             EXPECT_EQ(2, callback_count_);   // one from attach, one from data change

             // finished, stop event loops
             shutdown();
             disp().stop();
         };
      };
   }

   int callback_count_ = 0;
};


struct InvalidatedPropertyClient : simppl::dbus::Stub<Properties>
{
   InvalidatedPropertyClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Properties>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         data.attach() >> [this](const simppl::dbus::CallState& cs, int) {
             ++calls_;
             EXPECT_FALSE((bool)cs);
             EXPECT_STREQ(cs.what(), "simppl.dbus.Invalid");

             shutdown();
         };
      };
   }

   int calls_ = 0;
};


struct Server : simppl::dbus::Skeleton<Properties>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Properties>(d, rolename)
   {
      shutdown >> [this](){
         disp().stop();
      };

      set >> [this](int id, const std::string& str){
         ++calls_;

         auto new_props = props.value();
         new_props[(ident_t)id] = str;

         props = new_props;

         mayShutdown.notify(42);
      };

      // initialize attribute
      data = 4711;
      props = { { One, "One" }, { Two, "Two" } };
   }

   int calls_ = 0;
};


struct InvalidSetterServer : simppl::dbus::Skeleton<Properties>
{
   InvalidSetterServer(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Properties>(d, rolename)
   {
      shutdown >> [this](){
         disp().stop();
      };

      data >> [this](int newval){

          if (newval < 0)
             throw std::runtime_error("out of bounds");  // will be mapped to simppl.dbus.UnhandledException

          if (newval == 1)
             throw simppl::dbus::Error("Invalid.Argument");

          data = newval;
      };

      // initialize attribute
      data = 4711;
   }
};


struct NonCachingTestClient : simppl::dbus::Stub<Properties>
{
   NonCachingTestClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Properties>(d, "s")
   {
      connected >> [this](simppl::dbus::ConnectionState s){
         EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

         // never use operator= here, it's blocking!
         data.get_async() >> [this](const simppl::dbus::CallState& cs, int i){
             EXPECT_TRUE((bool)cs);
             EXPECT_EQ(i, 4711);

             data.attach() >> [this](const simppl::dbus::CallState& cs, int i){
                EXPECT_TRUE((bool)cs);

                if (i == 4711)
                {
                    set.async(42, "Huhu");
                }
                else
                   disp().stop();
             };
         };
      };
   }
};


struct NonCachingPropertyServer : simppl::dbus::Skeleton<Properties>
{
   NonCachingPropertyServer(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Properties>(d, rolename)
   {
      set >> [this](int id, const std::string& /*str*/){
         data.notify(id);
      };

      // initialize attributes callbacks
      data.on_read([](){
          return 4711;
      });

      props.on_read([](){
          std::map<ident_t, std::string> rc = { { One, "One" }, { Two, "Two" } };
          return rc;
      });
   }
};


struct ErrorThrowingPropertyServer : simppl::dbus::Skeleton<Properties>
{
   ErrorThrowingPropertyServer(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Properties>(d, rolename)
   {
      // initialize attributes callbacks
      data.on_read([](){
          throw simppl::dbus::Error("Not.Available");

          // complete lambda with return value
          return 42;
      });

      // stop thread
      shutdown >> [this](){
         disp().stop();
      };
   }
};


struct InvalidatedPropertyServer : simppl::dbus::Skeleton<Properties>
{
   InvalidatedPropertyServer(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Skeleton<Properties>(d, "s")
    , countdown_(3)
   {
      // initialize attributes callbacks
      data.on_read([](){

          throw simppl::dbus::Error("simppl.dbus.Invalid");

          // complete lambda with return value
          return 42;
      });

      // just send message
      shutdown >> [this](){

         if(--countdown_ > 0)
         {
            data.invalidate();   // sending error simppl.dbus.Invalid
         }
         else
            disp().stop();
      };
   }

   int countdown_;
};


}   // anonymous namespace


TEST(Properties, attr)
{
   simppl::dbus::Dispatcher d("bus:session");
   Client c(d);
   Server s(d, "s");

   d.run();

   EXPECT_EQ(2, c.callback_count_);   // one for the attach, then one more update
   EXPECT_EQ(3, s.calls_);
}


TEST(Properties, multiple_attach)
{
   simppl::dbus::Dispatcher d("bus:session");
   MultiClient c1(d, true);
   MultiClient c2(d, false);
   Server s(d, "s");

   d.run();

   EXPECT_EQ(2, c1.callback_count_);
   EXPECT_EQ(0, c2.callback_count_);
}


namespace
{
    void blockrunner()
    {
       simppl::dbus::Dispatcher d("bus:session");
       Server s(d, "s");

       d.run();
    }
}


TEST(Properties, blocking_set)
{
   simppl::dbus::Dispatcher d("bus:session");

   std::thread t(blockrunner);

   // wait for server to get ready
   std::this_thread::sleep_for(200ms);

   simppl::dbus::Stub<Properties> c(d, "s");

   c.data = 5555;
   int val = c.data.get();

   EXPECT_EQ(val, 5555);

   c.shutdown();   // stop server
   t.join();
}


TEST(Properties, blocking_get)
{
   simppl::dbus::Dispatcher d("bus:session");

   std::thread t(blockrunner);

   // wait for server to get ready
   std::this_thread::sleep_for(200ms);

   simppl::dbus::Stub<Properties> c(d, "s");

   int val = c.data.get();

   EXPECT_EQ(val, 4711);

   c.shutdown();   // stop server
   t.join();
}


TEST(Properties, set)
{
   simppl::dbus::Dispatcher d("bus:session");

   Server s(d, "s");
   SetterClient c(d);

   d.run();
}


TEST(Properties, get_async)
{
   simppl::dbus::Dispatcher d("bus:session");

   Server s(d, "s");
   GetterClient c(d);

   d.run();
}


TEST(Properties, invalid_set)
{
   simppl::dbus::Dispatcher d("bus:session");

   InvalidSetterServer s(d, "s");
   InvalidSetterClient c(d);

   d.run();
}


TEST(Properties, non_caching)
{
   simppl::dbus::Dispatcher d("bus:session");

   NonCachingPropertyServer s(d, "s");
   NonCachingTestClient c(d);

   d.run();
}


TEST(Properties, blocking_get_error)
{
   simppl::dbus::Dispatcher d("bus:session");

   std::thread t([](){

       simppl::dbus::Dispatcher d("bus:session");
       ErrorThrowingPropertyServer s(d, "s");

       d.run();
   });

   // wait for server to get ready
   std::this_thread::sleep_for(200ms);

   simppl::dbus::Stub<Properties> c(d, "s");

   try
   {
      int val = c.data.get();
      (void)val;

      // never arrive here!
      EXPECT_FALSE(true);
   }
   catch(simppl::dbus::Error& err)
   {
       EXPECT_STREQ("Not.Available", err.name());
   }

   c.shutdown();   // stop server
   t.join();
}


TEST(Properties, get_async_error)
{
   simppl::dbus::Dispatcher d("bus:session");

   ErrorThrowingPropertyServer s(d, "s");
   GetterErrorClient c(d);

   d.run();
}


TEST(Properties, invalidate)
{
   simppl::dbus::Dispatcher d("bus:session");

   InvalidatedPropertyServer s(d);
   InvalidatedPropertyClient c(d);

   d.run();
   EXPECT_EQ(c.calls_, 3);
}
