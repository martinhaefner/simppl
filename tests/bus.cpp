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


/// Left and Right share signal and property names, but not their types
INTERFACE(Left)
{
   Signal<int> changed;
   Property<int> value;

   Left()
    : INIT(changed)
    , INIT(value)
   {
      // NOOP
   }
};


INTERFACE(Right)
{
   Signal<std::string> changed;
   Property<std::string> value;

   Right()
    : INIT(changed)
    , INIT(value)
   {
      // NOOP
   }
};


/// own signal that happens to be named like the standard properties signal
INTERFACE(OwnPropertiesChanged)
{
   Signal<int> PropertiesChanged;

   OwnPropertiesChanged()
    : INIT(PropertiesChanged)
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
   simppl::dbus::Stub<org::freedesktop::Bus> bus(d, "org.freedesktop.DBus", "/org/freedesktop/DBus");

   EXPECT_STREQ("org.freedesktop.DBus", bus.iface());

   EXPECT_TRUE(bus.NameHasOwner("org.freedesktop.DBus"));
   EXPECT_FALSE(bus.NameHasOwner("simppl.test.bus.not_there"));

   std::vector<std::string> names = bus.ListNames();
   EXPECT_NE(names.end(), std::find(names.begin(), names.end(), "org.freedesktop.DBus"));

   std::string id = bus.GetId();
   EXPECT_FALSE(id.empty());
}


TEST(Bus, connected)
{
   simppl::dbus::Dispatcher d("bus:session");
   simppl::dbus::Stub<org::freedesktop::Bus> bus(d, "org.freedesktop.DBus", "/org/freedesktop/DBus");

   bool connected = false;

   bus.connected >> [&](simppl::dbus::ConnectionState s){
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
   simppl::dbus::Stub<org::freedesktop::Bus> bus(d, "org.freedesktop.DBus", "/org/freedesktop/DBus");

   // service lives on its own connection, dropping that connection
   // drops the busname
   std::unique_ptr<simppl::dbus::Dispatcher> service_disp;
   std::unique_ptr<Server> service;

   std::unique_ptr<simppl::dbus::Stub<test::bus::Plain>> client;

   bool appeared = false;
   bool client_connected = false;
   bool client_disconnected = false;
   bool vanished = false;

   bus.NameOwnerChanged.attach() >> [&](const std::string& name, const std::string& old_owner, const std::string& new_owner){

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


/**
 * Several user stubs of the bus next to the dispatcher's internal one:
 * every stub gets every signal, attaching or dropping one stub does not
 * affect the others, and creating a stub does not re-notify existing ones.
 */
TEST(Bus, second_stub)
{
   simppl::dbus::Dispatcher d("bus:session");

   auto first = std::make_unique<simppl::dbus::Stub<org::freedesktop::Bus>>(d, "org.freedesktop.DBus", "/org/freedesktop/DBus");

   int first_connected = 0;
   first->connected >> [&](simppl::dbus::ConnectionState s){
      if (s == simppl::dbus::ConnectionState::Connected)
         ++first_connected;
   };

   int first_changes = 0;
   first->NameOwnerChanged.attach() >> [&](const std::string& name, const std::string&, const std::string&){
      if (name == watched_busname)
         ++first_changes;
   };

   EXPECT_TRUE(run_until(d, [&](){ return first_connected == 1; }));

   // second stub after the first one is up and running
   simppl::dbus::Stub<org::freedesktop::Bus> second(d, "org.freedesktop.DBus", "/org/freedesktop/DBus");

   int second_connected = 0;
   second.connected >> [&](simppl::dbus::ConnectionState s){
      if (s == simppl::dbus::ConnectionState::Connected)
         ++second_connected;
   };

   int second_changes = 0;
   second.NameOwnerChanged.attach() >> [&](const std::string& name, const std::string&, const std::string&){
      if (name == watched_busname)
         ++second_changes;
   };

   EXPECT_TRUE(run_until(d, [&](){ return second_connected == 1; }));

   auto service_cycle = [&](){
      simppl::dbus::Dispatcher service_disp("bus:session");
      Server service(service_disp, watched_busname, watched_objectpath);
   };

   // appear + vanish -> 2 signals for each stub
   service_cycle();
   EXPECT_TRUE(run_until(d, [&](){ return first_changes == 2 && second_changes == 2; }));

   // drop one subscription and the stub itself, the other one keeps receiving
   first->NameOwnerChanged.detach();
   first.reset();

   service_cycle();
   EXPECT_TRUE(run_until(d, [&](){ return second_changes == 4; }));

   // dispatcher's own tracking is unaffected: a new client still connects
   bool client_connected = false;
   {
      simppl::dbus::Dispatcher service_disp("bus:session");
      Server service(service_disp, watched_busname, watched_objectpath);

      simppl::dbus::Stub<test::bus::Plain> client(d, watched_busname, watched_objectpath);
      client.connected >> [&](simppl::dbus::ConnectionState s){
         client_connected = (s == simppl::dbus::ConnectionState::Connected);
      };

      EXPECT_TRUE(run_until(d, [&](){ return client_connected; }));
   }

   // creating the second stub must not have re-notified the first one
   EXPECT_EQ(1, first_connected);
   EXPECT_EQ(1, second_connected);
}


namespace
{

constexpr const char* shared_objectpath = "/simppl/test/bus/shared";

}   // anonymous namespace


/**
 * Two interfaces on the same object path with equally named signals: a stub
 * only gets the signals of its own interface.
 */
TEST(SignalFilter, same_signal_name_other_interface)
{
   simppl::dbus::Dispatcher d("bus:session");

   simppl::dbus::Dispatcher left_disp("bus:session");
   simppl::dbus::Skeleton<test::bus::Left> left_server(left_disp, "simppl.test.bus.left", shared_objectpath);

   simppl::dbus::Dispatcher right_disp("bus:session");
   simppl::dbus::Skeleton<test::bus::Right> right_server(right_disp, "simppl.test.bus.right", shared_objectpath);

   simppl::dbus::Stub<test::bus::Left> left(d, "simppl.test.bus.left", shared_objectpath);
   simppl::dbus::Stub<test::bus::Right> right(d, "simppl.test.bus.right", shared_objectpath);

   std::vector<int> left_values;
   std::vector<std::string> right_values;

   left.changed.attach() >> [&](int i){ left_values.push_back(i); };
   right.changed.attach() >> [&](const std::string& str){ right_values.push_back(str); };

   left_server.changed.notify(42);
   right_server.changed.notify("hello");

   EXPECT_TRUE(run_until(d, [&](){ return !left_values.empty() && !right_values.empty(); }));

   // give misrouted signals a chance to arrive
   run_until(d, [](){ return false; }, 200ms);

   EXPECT_EQ(std::vector<int>{ 42 }, left_values);
   EXPECT_EQ(std::vector<std::string>{ "hello" }, right_values);
}


/**
 * Two interfaces on the same object path with equally named properties: a
 * stub only takes over property changes of its own interface.
 */
TEST(SignalFilter, same_property_name_other_interface)
{
   simppl::dbus::Dispatcher d("bus:session");

   simppl::dbus::Dispatcher left_disp("bus:session");
   simppl::dbus::Skeleton<test::bus::Left> left_server(left_disp, "simppl.test.bus.left", shared_objectpath);
   left_server.value = 1;

   simppl::dbus::Dispatcher right_disp("bus:session");
   simppl::dbus::Skeleton<test::bus::Right> right_server(right_disp, "simppl.test.bus.right", shared_objectpath);
   right_server.value = "one";

   simppl::dbus::Stub<test::bus::Left> left(d, "simppl.test.bus.left", shared_objectpath);
   simppl::dbus::Stub<test::bus::Right> right(d, "simppl.test.bus.right", shared_objectpath);

   std::vector<int> left_values;
   std::vector<std::string> right_values;

   left.value.attach() >> [&](const simppl::dbus::CallState& state, int i){
      EXPECT_TRUE((bool)state);
      left_values.push_back(i);
   };

   right.value.attach() >> [&](const simppl::dbus::CallState& state, const std::string& str){
      EXPECT_TRUE((bool)state);
      right_values.push_back(str);
   };

   // the servers' dispatchers are not running, so serve the initial Get
   // requests of attach() by stepping them along with the client
   auto step_all = [&](auto done, std::chrono::milliseconds timeout = 5000ms){
      left_disp.init();
      right_disp.init();
      return run_until(d, [&](){ left_disp.step(0ms); right_disp.step(0ms); return done(); }, timeout);
   };

   EXPECT_TRUE(step_all([&](){ return !left_values.empty() && !right_values.empty(); }));

   left_server.value = 2;
   right_server.value = "two";

   EXPECT_TRUE(step_all([&](){ return left_values.back() == 2 && right_values.back() == "two"; }));

   // give misrouted signals a chance to arrive
   step_all([](){ return false; }, 200ms);

   EXPECT_EQ((std::vector<int>{ 1, 2 }), left_values);
   EXPECT_EQ((std::vector<std::string>{ "one", "two" }), right_values);
}


/**
 * An interface's own signal named PropertiesChanged is a plain signal, not
 * a property change notification.
 */
TEST(SignalFilter, own_signal_named_properties_changed)
{
   simppl::dbus::Dispatcher d("bus:session");

   simppl::dbus::Dispatcher server_disp("bus:session");
   simppl::dbus::Skeleton<test::bus::OwnPropertiesChanged> server(server_disp, "simppl.test.bus.own", shared_objectpath);

   simppl::dbus::Stub<test::bus::OwnPropertiesChanged> stub(d, "simppl.test.bus.own", shared_objectpath);

   int received = 0;
   stub.PropertiesChanged.attach() >> [&](int i){ received = i; };

   server.PropertiesChanged.notify(4711);

   EXPECT_TRUE(run_until(d, [&](){ return received != 0; }));
   EXPECT_EQ(4711, received);
}


/**
 * Another client sends a faked NameOwnerChanged directly to us. It must
 * neither reach a user stub of the bus nor the dispatcher's own tracking.
 */
TEST(SignalFilter, faked_name_owner_changed)
{
   constexpr const char* fake_busname = "simppl.test.bus.fake";

   simppl::dbus::Dispatcher d("bus:session");
   simppl::dbus::Stub<org::freedesktop::Bus> bus(d, "org.freedesktop.DBus", "/org/freedesktop/DBus");

   std::vector<std::string> changes;
   bus.NameOwnerChanged.attach() >> [&](const std::string& name, const std::string&, const std::string&){
      changes.push_back(name);
   };

   bool fake_connected = false;
   simppl::dbus::Stub<test::bus::Plain> fake_client(d, fake_busname, "/");
   fake_client.connected >> [&](simppl::dbus::ConnectionState s){
      fake_connected = (s == simppl::dbus::ConnectionState::Connected);
   };

   // the attacker: a plain connection forging the bus driver's signal
   simppl::dbus::Dispatcher attacker("bus:session");
   {
      DBusMessage* msg = dbus_message_new_signal("/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged");

      const char* name = fake_busname;
      const char* old_owner = "";
      const char* new_owner = ":1.4711";
      dbus_message_append_args(msg, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &old_owner, DBUS_TYPE_STRING, &new_owner, DBUS_TYPE_INVALID);

      // unicast: delivered regardless of any match rules
      dbus_message_set_destination(msg, dbus_bus_get_unique_name(&d.connection()));

      dbus_connection_send(&attacker.connection(), msg, nullptr);
      dbus_connection_flush(&attacker.connection());
      dbus_message_unref(msg);
   }

   // a real change afterwards, to know when we're done
   {
      simppl::dbus::Dispatcher service_disp("bus:session");
      Server service(service_disp, watched_busname, watched_objectpath);
   }

   EXPECT_TRUE(run_until(d, [&](){ return std::count(changes.begin(), changes.end(), watched_busname) == 2; }));

   EXPECT_EQ(0, std::count(changes.begin(), changes.end(), fake_busname));
   EXPECT_FALSE(fake_connected);
}
