#include <iostream>

#include "echoservice.h"

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/string.h"


class MyEchoClient : public simppl::dbus::Stub<simppl::example::EchoService>
{

public:
    MyEchoClient(simppl::dbus::Dispatcher& disp)
     : simppl::dbus::Stub<simppl::example::EchoService>(disp, "myEcho")
    {
        connected >> [this](simppl::dbus::ConnectionState st){handle_connected(st);};
    }

private:
    void handle_connected(simppl::dbus::ConnectionState st)
    {
        if (st == simppl::dbus::ConnectionState::Connected)
        {
            echo.async("Hello World!") >> [this](const simppl::dbus::CallState st, const std::string& echo_string)
            {
                if (st)
                {
                    std::cout << "Server says '" << echo_string << "'" << std::endl;
                }
                else
                    std::cout << "Got error: " << st.what() << std::endl;
                    
                disp().stop();
            };
        }
    }
};

class MyEcho : public simppl::dbus::Skeleton<simppl::example::EchoService>
{
public:
    MyEcho(simppl::dbus::Dispatcher& disp)
     : simppl::dbus::Skeleton<simppl::example::EchoService>(disp, "myEcho")
    {
        echo >> [this](const std::string& echo_string)
        {
            std::cout << "Client says '" << echo_string << "'" << std::endl;
            respond_with(echo(echo_string));
        };
    }
};


int main(int argc, char** argv)
{
   simppl::dbus::Dispatcher disp("bus:session");
    
   if (argc == 1)
   {
      MyEchoClient client(disp);
      MyEcho service(disp);
      disp.run();
   }
   else if (!strcmp(argv[1], "--server"))
   {
      MyEcho service(disp);
      disp.run();
   }
   else if (!strcmp(argv[1], "--client"))
   {
      MyEchoClient client(disp);
      disp.run();
   }
   
   return EXIT_SUCCESS;
}
