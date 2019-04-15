#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#include <functional>
#include <thread>


using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{


INTERFACE(Adder)
{
   Method<in<int>, in<int>, out<int>> Add;

   Adder()
    : INIT(Add)
   {}
};


INTERFACE(Multiplier)
{
   Method<in<int>, in<int>, out<int>> Multiply;

   Multiplier()
    : INIT(Multiply)
   {}
};


}  // namespace test


namespace
{


class MathServer {
public:
    MathServer()
      : disp_("bus:session")
      , math_(disp_)
    {
        worker_ = std::thread([this] {
            disp_.run();
        });
    }

    ~MathServer() {
        disp_.stop();
        worker_.join();
    }

private:
    struct MathService : simppl::dbus::Skeleton<test::Adder, test::Multiplier>
    {
        MathService(simppl::dbus::Dispatcher& d)
        : simppl::dbus::Skeleton<test::Adder, test::Multiplier>(d, "simppl.test", "/")
        {
            Add >> [this] (int a, int b) {
                respond_with(Add(a + b));
            };

            Multiply >> [this] (int a, int b) {
                respond_with(Multiply(a * b));
            };
        }
    };

    simppl::dbus::Dispatcher disp_;
    MathService math_;
    std::thread worker_;
};


}   // anonymous namespace


TEST(MultiInterface, adder_interface)
{
    struct AdderClient : simppl::dbus::Stub<test::Adder>
    {
        AdderClient(simppl::dbus::Dispatcher& d)
            : simppl::dbus::Stub<test::Adder>(d, "simppl.test", "/")
        {
            connected >> [this](simppl::dbus::ConnectionState s) {
                EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

                Add.async(1, 1) >> [&](simppl::dbus::CallState state, int result) {
                    EXPECT_TRUE(static_cast<bool>(state));
                    EXPECT_EQ(result, 2);
                    disp().stop();
                };
            };
        }
    };

    MathServer server;
    simppl::dbus::Dispatcher disp("bus:session");
    AdderClient client(disp);
    disp.run();
}


TEST(MultiInterface, multiplier_interface)
{
    struct MultiplierClient : simppl::dbus::Stub<test::Multiplier>
    {
        MultiplierClient(simppl::dbus::Dispatcher& d)
            : simppl::dbus::Stub<test::Multiplier>(d, "simppl.test", "/")
        {
            connected >> [this](simppl::dbus::ConnectionState s) {
                EXPECT_EQ(simppl::dbus::ConnectionState::Connected, s);

                Multiply.async(1, 1) >> [&](simppl::dbus::CallState state, int result) {
                    EXPECT_TRUE(static_cast<bool>(state));
                    EXPECT_EQ(result, 1);
                    disp().stop();
                };
            };
        }
    };

    MathServer server;
    simppl::dbus::Dispatcher disp("bus:session");
    MultiplierClient client(disp);
    disp.run();
}
