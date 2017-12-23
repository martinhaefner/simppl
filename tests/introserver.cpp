#include "simppl/skeleton.h"
#include "simppl/interface.h"
#include "simppl/map.h"
#include "simppl/vector.h"
#include "simppl/struct.h"
#include "simppl/string.h"
#include "simppl/variant.h"


using simppl::dbus::in;
using simppl::dbus::out;


namespace test
{

enum ident_t {
   One=1, Two, Three, Four, Five, Six
};


struct NumericEntry
{
   typedef simppl::dbus::make_serializer<int, int, int>::type serializer_type;

   NumericEntry() = default;
   NumericEntry(int min, int max, int step)
    : min_(min)
    , max_(max)
    , step_(step)
   {
      // NOOP
   }

   int min_;
   int max_;
   int step_;
};


struct StringEntry
{
   typedef simppl::dbus::make_serializer<int, int>::type serializer_type;

   StringEntry() = default;
   StringEntry(int min, int max)
    : min_(min)
    , max_(max)
   {
      // NOOP
   }

   int min_;
   int max_;
};


struct ComboEntry
{
   typedef simppl::dbus::make_serializer<std::vector<std::string>>::type serializer_type;

   ComboEntry() = default;
   ComboEntry(std::vector<std::string> entries)
    : entries_(entries)
   {
      // NOOP
   }

   std::vector<std::string> entries_;
};


struct Menu
{
   typedef simppl::Variant<NumericEntry, StringEntry, ComboEntry, Menu> entry_type;

   typedef std::map<std::string,
                    std::tuple<int/*=id*/, entry_type>> menu_entries_type;

   typedef simppl::dbus::make_serializer<menu_entries_type>::type serializer_type;

   menu_entries_type entries_;
};


INTERFACE(Properties)
{
   Method<in<int>, in<std::string>> set;
   Method<> shutdown;
   Method<> trigger_data;

   Method<out<Menu>> get_all;

   Property<int, simppl::dbus::ReadWrite|simppl::dbus::Notifying> data;
   Property<std::map<ident_t, std::string>> props;

   Signal<int> mayShutdown;
   Signal<> hi;

   inline
   Properties()
    : INIT(set)
    , INIT(shutdown)
    , INIT(trigger_data)
    , INIT(get_all)
    , INIT(data)
    , INIT(props)
    , INIT(mayShutdown)
    , INIT(hi)
   {
      // NOOP
   }
};

}

using namespace test;


struct Server : simppl::dbus::Skeleton<Properties>
{
   Server(simppl::dbus::Dispatcher& d, const char* rolename)
    : simppl::dbus::Skeleton<Properties>(d, rolename)
   {
      shutdown >> [this](){
         disp().stop();
      };
      
      set >> [this](int id, const std::string& str){
         ++calls_;

         auto new_props = props.value();
         new_props[(ident_t)id] = str;

         props = new_props;

         mayShutdown.notify(42);
         hi.notify();
      };
      
      get_all >> [this](){
         Menu mainmenu;

         Menu printing;
         printing.entries_["Speed"] = std::make_tuple(1, Menu::entry_type(NumericEntry(50, 250, 25)));
         printing.entries_["Heat Level"] = std::make_tuple(2, Menu::entry_type(NumericEntry(-20, 20, 1)));

         Menu display;
         display.entries_["Orientation"] = std::make_tuple(3, Menu::entry_type(ComboEntry({"0", "90", "180", "270"})));
         display.entries_["Powersave"] = std::make_tuple(4, Menu::entry_type(NumericEntry(0, 10, 1)));

         Menu settings;
         settings.entries_["Printing"] = std::make_tuple(9, Menu::entry_type(printing));
         settings.entries_["Display"] = std::make_tuple(10, Menu::entry_type(display));

         Menu security;
         security.entries_["PIN"] = std::make_tuple(7, Menu::entry_type(StringEntry(6, 255)));
         security.entries_["FTP Password"] = std::make_tuple(8, Menu::entry_type(StringEntry(6, 255)));

         mainmenu.entries_["Settings"] = std::make_tuple(5, Menu::entry_type(settings));
         mainmenu.entries_["Security"] = std::make_tuple(6, Menu::entry_type(security));

         respond_with(get_all(mainmenu));
      };
      
      trigger_data >> [this](){
         data = 8888;
      };

      // initialize attribute
      data = 4711;
      props = { { One, "One" }, { Two, "Two" } };
   }

   int calls_ = 0;
};


int main()
{
   simppl::dbus::Dispatcher d("bus:session");
   Server s(d, "s");

   d.run();

   return 0;
}

