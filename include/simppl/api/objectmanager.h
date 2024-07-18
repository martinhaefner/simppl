#ifndef __SIMPPL_ORG_FREEDESKTOP_DBUS_OBJECTMANAGER_H__
#define __SIMPPL_ORG_FREEDESKTOP_DBUS_OBJECTMANAGER_H__


#include "simppl/interface.h"
#include "simppl/map.h"
#include "simppl/vector.h"
#include "simppl/objectpath.h"
#include "simppl/any.h"


namespace org
{
    namespace freedesktop
    {
        namespace DBus
        {
            using simppl::dbus::out;


            typedef
                std::map<simppl::dbus::ObjectPath,                 ///< object registration path
                    std::map<std::string,                          ///< interface name
                        std::map<std::string, simppl::dbus::Any>   ///< properties
                            >
                        > managed_objects_t;


            INTERFACE(ObjectManager)
            {
                Method<out<managed_objects_t>> GetManagedObjects;

                Signal<simppl::dbus::ObjectPath, std::map<std::string, std::map<std::string, simppl::dbus::Any>>> InterfacesAdded;
                Signal<simppl::dbus::ObjectPath, std::vector<std::string>>                                        InterfacesRemoved;

                ObjectManager()
                 : INIT(GetManagedObjects)
                 , INIT(InterfacesAdded)
                 , INIT(InterfacesRemoved)
                {
                    // NOOP
                }
            };
        }
    }
}


#endif   // __SIMPPL_ORG_FREEDESKTOP_DBUS_OBJECTMANAGER_H__
