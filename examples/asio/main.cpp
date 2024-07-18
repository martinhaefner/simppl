#include <iostream>

#include <boost/asio.hpp>

#include "simppl/skeleton.h"

#include "dbusexecutor.h"


namespace example {

INTERFACE(Hello)
{
    Method<simppl::dbus::oneway> world;

    Hello()
     : INIT(world)
    {
        // NOOP
    }
};

}   // example


int main()
{
    // the event loop
    boost::asio::io_context ctx;

    // simppl dbus glue code
    DBusExecutor exec(ctx);

    // the server object - call it via
    //
    // dbus-send --session --dest=example.Hello.hello /example/Hello/hello example.Hello.world
    //
    simppl::dbus::Skeleton<example::Hello> hello(exec.dispatcher(), "hello");

    // method implementation
    hello.world >> [](){
        std::cout << "Hello World!" << std::endl;
    };

    return ctx.run();
}