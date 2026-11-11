#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"
#include "simppl/api/dbus.h"

#include <algorithm>
#include <chrono>
#include <memory>


using namespace std::literals::chrono_literals;

using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{

namespace bus
{

INTERFACE(Plain)
{
   Method<in<int>, out<int>> twice;

   Plain()
    : INIT(twice)
   {
      // NOOP
   }
};


INTERFACE(Renamed, "simppl.test.OtherName")
{
   Method<in<int>, out<int>> negate;

   Renamed()
    : INIT(negate)
   {
      // NOOP
   }
};

}   // namespace bus

}   // namespace test



namespace
{

constexpr const char* watched_busname = "simppl.test.bus.watched";
constexpr const char* watched_objectpath = "/simppl/test/bus/watched";


struct Server : simppl::dbus::Skeleton<test::bus::Plain, test::bus::Renamed>
{
   Server(simppl::dbus::Dispatcher& d, const char* busname, const char* objectpath)
    : simppl::dbus::Skeleton<test::bus::Plain, test::bus::Renamed>(d, busname, objectpath)
   {
      twice >> [this](int i){
         respond_with(twice(2*i));
      };

      negate >> [this](int i){
         respond_with(negate(-i));
      };
   }
};


/// step the dispatcher until @c done is true or a timeout expires
template<typename PredT>
bool run_until(simppl::dbus::Dispatcher& d, PredT done, std::chrono::milliseconds timeout = 5000ms)
{
   d.init();

   auto deadline = std::chrono::steady_clock::now() + timeout;

   while(!done() && std::chrono::steady_clock::now() < deadline)
      d.step(50ms);

   return done();
}

}   // anonymous namespace


TEST(InterfaceName, derived_from_type)
{
   simppl::dbus::Dispatcher d("bus:session");
   simppl::dbus::Stub<test::bus::Plain> stub(d, "simppl.test.bus.any", "/");

   EXPECT_STREQ("test.bus.Plain", stub.iface());
}


TEST(InterfaceName, explicit_name)
{
   simppl::dbus::Dispatcher d("bus:session");
   simppl::dbus::Stub<test::bus::Renamed> stub(d, "simppl.test.bus.any", "/");

   EXPECT_STREQ("simppl.test.OtherName", stub.iface());
}


TEST(InterfaceName, explicit_name_from_role)
{
   simppl::dbus::Dispatcher d("bus:session");
   simppl::dbus::Stub<test::bus::Renamed> stub(d, "role");

   EXPECT_STREQ("simppl.test.OtherName", stub.iface());
   EXPECT_STREQ("simppl.test.OtherName.role", stub.busname().c_str());
   EXPECT_STREQ("/simppl/test/OtherName/role", stub.objectpath());
}


TEST(InterfaceName, skeleton_with_mixed_names)
{
   simppl::dbus::Dispatcher d("bus:session");
   Server s(d, "simppl.test.bus.mixed", "/simppl/test/bus/mixed");

   EXPECT_EQ("test.bus.Plain", s.iface(1));          // ifaces_[0] has the highest id
   EXPECT_EQ("simppl.test.OtherName", s.iface(0));

   simppl::dbus::Stub<test::bus::Plain> plain(d, "simppl.test.bus.mixed", "/simppl/test/bus/mixed");
   simppl::dbus::Stub<test::bus::Renamed> renamed(d, "simppl.test.bus.mixed", "/simppl/test/bus/mixed");

   int twice_result = 0;
   int negate_result = 0;

   plain.twice.async(21) >> [&](const simppl::dbus::CallState& state, int result){
      EXPECT_TRUE((bool)state);
      twice_result = result;
   };

   renamed.negate.async(7) >> [&](const simppl::dbus::CallState& state, int result){
      EXPECT_TRUE((bool)state);
      negate_result = result;
   };

   EXPECT_TRUE(run_until(d, [&](){ return twice_result != 0 && negate_result != 0; }));

   EXPECT_EQ(42, twice_result);
   EXPECT_EQ(-7, negate_result);
}


TEST(Bus, blocking_calls)
{
   simppl::dbus::Dispatcher d("bus:session");

   EXPECT_STREQ("org.freedesktop.DBus", d.bus().iface());

   EXPECT_TRUE(d.bus().NameHasOwner("org.freedesktop.DBus"));
   EXPECT_FALSE(d.bus().NameHasOwner("simppl.test.bus.not_there"));

   std::vector<std::string> names = d.bus().ListNames();
   EXPECT_NE(names.end(), std::find(names.begin(), names.end(), "org.freedesktop.DBus"));

   std::string id = d.bus().GetId();
   EXPECT_FALSE(id.empty());
}


TEST(Bus, connected)
{
   simppl::dbus::Dispatcher d("bus:session");

   bool connected = false;

   d.bus().connected >> [&](simppl::dbus::ConnectionState s){
      connected = (s == simppl::dbus::ConnectionState::Connected);
   };

   EXPECT_TRUE(run_until(d, [&](){ return connected; }));
}


/**
 * Service appears and vanishes. A stub created from within the user's
 * NameOwnerChanged callback must already be connected since the dispatcher's
 * own tracking runs first.
 */
TEST(Bus, name_owner_changed)
{
   simppl::dbus::Dispatcher d("bus:session");

   // service lives on its own connection, dropping that connection
   // drops the busname
   std::unique_ptr<simppl::dbus::Dispatcher> service_disp;
   std::unique_ptr<Server> service;

   std::unique_ptr<simppl::dbus::Stub<test::bus::Plain>> client;

   bool appeared = false;
   bool client_connected = false;
   bool client_disconnected = false;
   bool vanished = false;

   d.bus().NameOwnerChanged.attach() >> [&](const std::string& name, const std::string& old_owner, const std::string& new_owner){

      if (name != watched_busname)
         return;

      if (old_owner.empty())
      {
         appeared = true;
         EXPECT_FALSE(new_owner.empty());

         client.reset(new simppl::dbus::Stub<test::bus::Plain>(d, watched_busname, watched_objectpath));

         client->connected >> [&](simppl::dbus::ConnectionState s){

            if (s == simppl::dbus::ConnectionState::Connected)
            {
               client_connected = true;

               // now let the service go away
               service.reset();
               service_disp.reset();
            }
            else
               client_disconnected = true;
         };
      }
      else if (new_owner.empty())
      {
         vanished = true;

         // tracking is done before the user callback
         EXPECT_TRUE(client_disconnected);
      }
   };

   service_disp.reset(new simppl::dbus::Dispatcher("bus:session"));
   service.reset(new Server(*service_disp, watched_busname, watched_objectpath));

   EXPECT_TRUE(run_until(d, [&](){ return vanished; }));

   EXPECT_TRUE(appeared);
   EXPECT_TRUE(client_connected);
   EXPECT_TRUE(client_disconnected);
   EXPECT_TRUE(vanished);
}


/**
 * A signal callback destroys another stub listening on the same object
 * path: the destroyed stub must not be called anymore.
 */
TEST(Bus, destroy_stub_from_signal_callback)
{
   simppl::dbus::Dispatcher d("bus:session");

   auto first = std::make_unique<simppl::dbus::Stub<org::freedesktop::Bus>>(d, "org.freedesktop.DBus", "/org/freedesktop/DBus");
   auto second = std::make_unique<simppl::dbus::Stub<org::freedesktop::Bus>>(d, "org.freedesktop.DBus", "/org/freedesktop/DBus");

   bool first_called = false;
   bool second_called = false;

   first->NameOwnerChanged.attach() >> [&](const std::string& name, const std::string&, const std::string&){

      if (name == watched_busname)
      {
         first_called = true;
         second.reset();
      }
   };

   second->NameOwnerChanged.attach() >> [&](const std::string& name, const std::string&, const std::string&){

      if (name == watched_busname)
         second_called = true;
   };

   {
      simppl::dbus::Dispatcher service_disp("bus:session");
      Server service(service_disp, watched_busname, watched_objectpath);

      EXPECT_TRUE(run_until(d, [&](){ return first_called; }));
   }

   EXPECT_TRUE(first_called);
   EXPECT_FALSE(second_called);

   // the remaining stub still gets the vanishing service
   first_called = false;
   EXPECT_TRUE(run_until(d, [&](){ return first_called; }));
}
