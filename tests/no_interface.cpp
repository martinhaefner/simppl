#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"
#include "simppl/string.h"

#include <functional>
#include <thread>


using simppl::dbus::out;


namespace org
{

namespace freedesktop
{

namespace DBus
{

INTERFACE(Introspectable)
{
   Method<out<std::string>> Introspect;

   Introspectable()
    : INIT(Introspect)
   {}
};

}   // namespace DBus

}   // namespace freedesktop

}   // namespace org


namespace
{


class NoInterfaceServer {
public:
    NoInterfaceServer()
      : disp_("bus:session")
      , empty_(disp_, "simppl.test", "/")
    {
        worker_ = std::thread([this] {
            disp_.run();
        });
    }

    ~NoInterfaceServer() {
        disp_.stop();
        worker_.join();
    }

private:
    simppl::dbus::Dispatcher disp_;
    simppl::dbus::Skeleton<> empty_;
    std::thread worker_;
};


}   // anonymous namespace


TEST(NoInterface, introspect)
{
    struct IntrospectClient : simppl::dbus::Stub<org::freedesktop::DBus::Introspectable>
    {
        IntrospectClient(simppl::dbus::Dispatcher& d)
            : simppl::dbus::Stub<org::freedesktop::DBus::Introspectable>(d, "simppl.test", "/")
        {
            connected >> [this](simppl::dbus::ConnectionState s) {
                EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

                Introspect.async() >> [&](simppl::dbus::CallState state, std::string result) {
                    EXPECT_TRUE(static_cast<bool>(state));
                    EXPECT_EQ(result, "<?xml version=\"1.0\" ?>\n"
                                      "<node name=\"/\">\n"
                                      "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
                                      "    <method name=\"Introspect\">\n"
                                      "      <arg name=\"data\" type=\"s\" direction=\"out\"/>\n"
                                      "    </method>\n"
                                      "  </interface>\n"
                                      "</node>\n");
                    disp().stop();
                };
            };
        }
    };

    NoInterfaceServer server;
    simppl::dbus::Dispatcher disp("bus:session");
    IntrospectClient client(disp);
    disp.run();
}
