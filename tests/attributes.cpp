#include <gtest/gtest.h>

// just check if we get compilation errors with validation...
#define SIMPPL_HAVE_VALIDATION 1

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <thread>


using namespace std::placeholders;

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

INTERFACE(Attributes)
{
   Request<in<int>, in<std::string>> set;
   Request<simppl::dbus::oneway> shutdown;

   Attribute<int, simppl::dbus::ReadWrite|simppl::dbus::Notifying> data;
   Attribute<std::map<ident_t, std::string>> props;

   Signal<int> mayShutdown;

   inline
   Attributes()
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


struct Client : simppl::dbus::Stub<Attributes>
{
   Client(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Attributes>(d, "s")
   {
      connected >> std::bind(&Client::handleConnected, this, _1);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);
      props.attach() >> std::bind(&Client::handleProps, this, _1, _2);

      set.async(Four, "Four");
   }


   void handleProps(simppl::dbus::CallState, const std::map<ident_t, std::string>& props)
   {
      ++callback_count_;

      if (callback_count_ == 1)   // the property Get(...) from the attach
      {
         EXPECT_EQ(2, props.size());
         EXPECT_EQ(2, this->props.value().size());

         EXPECT_TRUE(props.find(Four) == props.end());
      }
      else if (callback_count_ == 2)   // the response on the assignment within the set(...) request
      {
         EXPECT_EQ(3, props.size());
         EXPECT_EQ(3, this->props.value().size());

         EXPECT_TRUE(props.find(Four) != props.end());

         // no more signals on this client
         this->props.detach();

         mayShutdown.attach() >> std::bind(&Client::aboutToShutdown, this, _1);

         // one more roundtrip to see if further signals arrive...
         set.async(Five, "Five");
         set.async(Six, "Six");
      }
   }


   void aboutToShutdown(int)
   {
      if (++count_ == 2)
         shutdown();
   }


   int callback_count_ = 0;
   int count_ = 0;
};


struct MultiClient : simppl::dbus::Stub<Attributes>
{
   MultiClient(simppl::dbus::Dispatcher& d, bool attach)
    : simppl::dbus::Stub<Attributes>(d, "s")
    , attach_(attach)
   {
      connected >> std::bind(&MultiClient::handleConnected, this, _1);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

      if (attach_)
      {
         props.attach() >> std::bind(&MultiClient::handleProps, this, _1, _2);
         set.async(Four, "Four");
      }
   }


   void handleProps(simppl::dbus::CallState, const std::map<ident_t, std::string>& props)
   {
      ++callback_count_;

      if (callback_count_ == 1)
      {
         EXPECT_EQ(2, props.size());
         EXPECT_EQ(2, this->props.value().size());

         EXPECT_TRUE(props.find(Four) == props.end());
      }
      else if (callback_count_ == 2)
      {
         EXPECT_EQ(3, props.size());
         EXPECT_EQ(3, this->props.value().size());

         EXPECT_TRUE(props.find(Four) != props.end());

         shutdown();
      }
   }

   bool attach_;
   int callback_count_ = 0;
};


struct SetterClient : simppl::dbus::Stub<Attributes>
{
   SetterClient(simppl::dbus::Dispatcher& d)
    : simppl::dbus::Stub<Attributes>(d, "s")
   {
      connected >> std::bind(&SetterClient::handleConnected, this, _1);
   }


   void handleConnected(simppl::dbus::ConnectionState s)
   {
      EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);
      data.attach() >> std::bind(&SetterClient::handleData, this, _1, _2);

      // never use operator= here, it's blocking!
      // FIXME change set_async interface to same way as async requests
      data.set_async(5555, [this](simppl::dbus::CallState cs){
          EXPECT_TRUE((bool)cs);

          disp().stop();
      });
   }


   void handleData(simppl::dbus::CallState, int i)
   {
      ++callback_count_;

      if (callback_count_ == 1)   // the property Get(...) from the attach
      {
         EXPECT_EQ(4711, i);
      }
      else if (callback_count_ == 2)   // the response on the assignment
      {
         EXPECT_EQ(5555, i);
      }
   }

   int callback_count_ = 0;
};


struct Server : simppl::dbus::Skeleton<Attributes>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Attributes>(d, rolename)
   {
      shutdown >> std::bind(&Server::handleShutdown, this);
      set >> std::bind(&Server::handleSet, this, _1, _2);

      // initialize attribute
      data = 4711;
      props = { { One, "One" }, { Two, "Two" } };
   }


   void handleShutdown()
   {
      disp().stop();
   }


   void handleSet(int id, const std::string& str)
   {
      ++calls_;

      auto new_props = props.value();
      new_props[(ident_t)id] = str;

      props = new_props;

      mayShutdown.emit(42);
   }

   int calls_ = 0;
};


}   // anonymous namespace


TEST(Attributes, attr)
{
   simppl::dbus::Dispatcher d("bus:session");
   Client c(d);
   Server s(d, "s");

   d.run();

   EXPECT_EQ(2, c.callback_count_);   // one for the attach, then one more update
   EXPECT_EQ(3, s.calls_);
}


TEST(Attributes, multiple_attach)
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


TEST(Attributes, blocking_set)
{
   simppl::dbus::Dispatcher d("bus:session");

   std::thread t(blockrunner);

   simppl::dbus::Stub<Attributes> c(d, "s");

   c.connect();

   c.data = 5555;
   int val = c.data.get();

   EXPECT_EQ(val, 5555);

   c.shutdown();   // stop server
   t.join();
}


TEST(Attributes, set)
{
   simppl::dbus::Dispatcher d("bus:session");

   Server s(d, "s");
   SetterClient c(d);

   d.run();
}
