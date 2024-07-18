#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <functional>
#include <thread>
#include <chrono>


using namespace std::literals::chrono_literals;

using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{


INTERFACE(One)
{
   Property<std::string> Str;
   Property<int>         Int;

   One()
    : INIT(Str)
    , INIT(Int)
   {}
};


INTERFACE(Two)
{
   Property<double> Double;
   Property<int>    Int;

   Two()
    : INIT(Double)
    , INIT(Int)
   {}
};


INTERFACE(Three)
{
   Property<simppl::dbus::ObjectPath> Obj;

   Three()
    : INIT(Obj)
   {}
};


INTERFACE(Server)
{
   Method<simppl::dbus::oneway> TriggerAddition;
   Method<simppl::dbus::oneway> TriggerRemoval;

   Server()
    : INIT(TriggerAddition)
    , INIT(TriggerRemoval)
   {}
};


}  // namespace test


TEST(ObjectManager, simple)
{
    simppl::dbus::Dispatcher s("bus:session");

    std::thread t([&s](){
        simppl::dbus::Skeleton<org::freedesktop::DBus::ObjectManager> mgr(s, "s");

        simppl::dbus::Skeleton<test::One> o(s, "s");
        o.Str = "Hallo";
        o.Int = 42;

        mgr.add_managed_object(&o);

        simppl::dbus::Skeleton<test::Two, test::Three> t(s, "test.Toll", "/test/Super");
        t.Double = 3.1415;
        t.Int = 4711;
        t.Obj = "/Super/Toll";

        mgr.add_managed_object(&t);

        s.run();
    });

    std::this_thread::sleep_for(200ms);

    simppl::dbus::Dispatcher c("bus:session");

    simppl::dbus::Stub<org::freedesktop::DBus::ObjectManager> clnt(c, "s");

    auto objs = clnt.GetManagedObjects();
    EXPECT_EQ(2, objs.size());

    s.stop();
    t.join();
}


TEST(ObjectManager, add_remove)
{
    simppl::dbus::Dispatcher s("bus:session");

    simppl::dbus::Skeleton<org::freedesktop::DBus::ObjectManager, test::Server> mgr(s, "test.server", "/");

    simppl::dbus::Skeleton<test::One> o(s, "s");
    o.Str = "Hallo";
    o.Int = 42;

    simppl::dbus::Skeleton<test::Two, test::Three> t(s, "test.Toll", "/test/Super");
    t.Double = 3.1415;
    t.Int = 4711;
    t.Obj = "/Super/Toll";


    mgr.TriggerAddition >> [&mgr, &o, &t](){

        mgr.add_managed_object(&o);
        mgr.add_managed_object(&t);
    };


    mgr.TriggerRemoval >> [&mgr, &o, &t](){

        mgr.remove_managed_object(&o);
        mgr.remove_managed_object(&t);
    };

    simppl::dbus::Stub<test::Server> sclnt(s, "test.server", "/");
    simppl::dbus::Stub<org::freedesktop::DBus::ObjectManager> clnt(s, "test.server", "/");

    int cnt = 0;
    const char* const ifaces[] = {
        "/test/One/s",
        "/test/Super"
    };

    clnt.InterfacesAdded.attach() >> [&cnt, &clnt, &sclnt, ifaces](const simppl::dbus::ObjectPath& p, const std::map<std::string, std::map<std::string, simppl::dbus::Any>>& props){

        EXPECT_STREQ(ifaces[cnt], p.path.c_str());

        // and remove them again
        if (++cnt == 2)
        {
            clnt.GetManagedObjects.async() >> [&sclnt](const simppl::dbus::CallState&, const org::freedesktop::DBus::managed_objects_t& objs){

                EXPECT_EQ(2, objs.size());

                sclnt.TriggerRemoval();
            };
        }
    };

    clnt.InterfacesRemoved.attach() >> [&cnt, &s, ifaces](const simppl::dbus::ObjectPath& p, const std::vector<std::string>& interfaces){

        EXPECT_STREQ(ifaces[cnt%2], p.path.c_str());

        if (++cnt == 4)
            s.stop();   // finished
    };

    sclnt.connected >> [&sclnt, &clnt](simppl::dbus::ConnectionState){

        // currently no managed objects
        clnt.GetManagedObjects.async() >> [&sclnt](const simppl::dbus::CallState&, const org::freedesktop::DBus::managed_objects_t& objs){

            EXPECT_EQ(0, objs.size());

            // add the interfaces
            sclnt.TriggerAddition();
        };
    };

    s.run();
}
