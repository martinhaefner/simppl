#include <iostream>
#include <iomanip>

#include "simppl/dispatcher.h"
#include "simppl/stub.h"
#include "simppl/vector.h"
#include "simppl/tuple.h"
#include "simppl/api/objectmanager.h"


struct print_value
{
    print_value(const simppl::dbus::Any& a)
     : a(a)
    {
        // NOOP
    }

    const simppl::dbus::Any& a;
};


std::ostream& operator<< (std::ostream& os, const print_value& value)
{
    if (value.a.is<int>())
    {
        os << value.a.as<int>();
    }
    else if (value.a.is<uint32_t>())
    {
        os << value.a.as<uint32_t>();
    }
    else if (value.a.is<bool>())
    {
        os << std::boolalpha << value.a.as<bool>() << std::noboolalpha;
    }
    else if (value.a.is<std::string>())
    {
        os << value.a.as<std::string>();
    }
    else if (value.a.is<simppl::dbus::ObjectPath>())
    {
        os << value.a.as<simppl::dbus::ObjectPath>().path;
    }
    else if (value.a.is<std::vector<std::string>>())
    {
        os << "[ ";

        for(auto& v : value.a.as<std::vector<std::string>>())
        {
            os << v << " ";
        }

        os << "]";
    }
    else if (value.a.is<std::vector<simppl::dbus::ObjectPath>>())
    {
        os << "[ ";

        for(auto& v : value.a.as<std::vector<simppl::dbus::ObjectPath>>())
        {
            os << v.path << " ";
        }

        os << "]";
    }
    else if (value.a.is<std::tuple<uint32_t, uint32_t>>())
    {
        auto v = value.a.as<std::tuple<uint32_t, uint32_t>>();

        os << "{ " << std::get<0>(v) << ", " << std::get<1>(v) << " }";
    }
    else if (value.a.is<std::vector<std::map<std::string, simppl::dbus::Any>>>())
    {
        auto vec = value.a.as<std::vector<std::map<std::string, simppl::dbus::Any>>>();

        os << "[ ... ]";   // leave this open for exercise
    }
    else if (value.a.is<std::vector<std::vector<uint8_t>>>())
    {
        os << "[ ";

        for(auto& vec : value.a.as<std::vector<std::vector<uint8_t>>>())
        {
            for(auto& v : vec)
            {
                os << std::setw(2) << std::hex << std::setfill('0') << (int)v << std::setfill(' ') << std::dec << " ";
            }
        }

        os << "]";
    }
    else
        os << "<unresolved>";

    return os;
}


int main()
{
    simppl::dbus::Dispatcher d("bus:system");
    simppl::dbus::Stub<org::freedesktop::DBus::ObjectManager> netmgr(d, "org.freedesktop.NetworkManager", "/org/freedesktop");

    try
    {
        auto objs = netmgr.GetManagedObjects();

        // objects
        for(auto& o : objs)
        {
            std::cout << o.first.path << std::endl;

            // interfaces
            for(auto& i : o.second)
            {
                std::cout << "    " << i.first << std::endl;

                // properties
                for(auto& p : i.second)
                {
                    std::cout << "        " << std::setw(30) << std::left << p.first << "\t" << print_value(p.second) << std::endl;
                }

                std::cout << std::endl;
            }

            std::cout << std::endl;
        }

        return EXIT_SUCCESS;
    }
    catch(std::exception& ex)
    {
        std::cerr << "Failed to access NetworkManager: " << ex.what() << std::endl;
    }

    return EXIT_FAILURE;
}
