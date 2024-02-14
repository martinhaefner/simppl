#include <gtest/gtest.h>

#include "simppl/stub.h"
#include "simppl/skeleton.h"
#include "simppl/interface.h"
#include "simppl/struct.h"
#include "simppl/map.h"
#include "simppl/string.h"


using simppl::dbus::out;


namespace test
{
   namespace variant
   {
      // FIXME make complete test with struct
      struct Hello
      {
         typedef simppl::dbus::make_serializer<int,std::string>::type serializer_type;

         int i;
         double d;
      };


      INTERFACE(VServer)
      {
         Method<out<std::map<std::string, std::variant<int,double,std::string>>>> getData;

         VServer()
          : INIT(getData)
         {
            // NOOP
         }
      };
   }
}


namespace {

   int constructs = 0;
   int destructs = 0;

   struct TestHelper
   {
      TestHelper()
      {
         ++constructs;
      }

      TestHelper(const TestHelper&)
      {
         ++constructs;
      }

      ~TestHelper()
      {
         ++destructs;
      }
   };


   struct Client : simppl::dbus::Stub<test::variant::VServer>
   {
      Client(simppl::dbus::Dispatcher& d)
       : simppl::dbus::Stub<test::variant::VServer>(d, "role")
      {
         connected >> [this](simppl::dbus::ConnectionState s){
            getData.async() >> [this](const simppl::dbus::CallState& state, const std::map<std::string, std::variant<int,double,std::string>>& mapping){
               EXPECT_EQ(3u, mapping.size());

               auto hello = mapping.find("Hello");
               auto world = mapping.find("World");
               auto toll = mapping.find("Tolle");

               EXPECT_NE(mapping.end(), hello);
               EXPECT_NE(mapping.end(), world);
               EXPECT_NE(mapping.end(), toll);

               EXPECT_EQ(42, std::get<int>(hello->second));
               EXPECT_EQ(4711, std::get<int>(world->second));
               EXPECT_EQ(std::string("Show"), std::get<std::string>(toll->second));

               disp().stop();
            };
         };
      }
   };

   struct Server : simppl::dbus::Skeleton<test::variant::VServer>
   {
      Server(simppl::dbus::Dispatcher& d)
       : simppl::dbus::Skeleton<test::variant::VServer>(d, "role")
      {
         getData >> [this](){
            std::map<std::string, std::variant<int,double,std::string>> mapping;
            mapping["Hello"] = 42;
            mapping["World"] = 4711;
            mapping["Tolle"] = std::string("Show");

            respond_with(getData(mapping));
         };
      }
   };
}


TEST(Variant, method)
{
   simppl::dbus::Dispatcher d("bus:session");
   Client c(d);
   Server s(d);

   d.run();
}
