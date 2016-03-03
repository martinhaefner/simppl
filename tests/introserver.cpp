#include "simppl/skeleton.h"
#include "simppl/dispatcher.h"
#include "simppl/interface.h"


using namespace std::placeholders;


namespace test
{

enum ident_t {
   One=1, Two, Three, Four, Five, Six
};


INTERFACE(Attributes)
{
   Request<int, std::string> set;
   Request<> shutdown;

   Attribute<int, simppl::dbus::ReadWrite|simppl::dbus::Notifying> data;
   Attribute<std::map<ident_t, std::string>> props;
   
   // FIXME without arg possible?
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


struct Server : simppl::dbus::Skeleton<Attributes>
{
   Server(const char* rolename)
    : simppl::dbus::Skeleton<Attributes>(rolename)
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


int main()
{
   simppl::dbus::Dispatcher d("dbus:session");
   Server s("s");

   d.addServer(s);
   d.run();

   return 0;
}

