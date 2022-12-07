#ifndef __ORG_FREEDESKTOP_DBUS_OBJECTMANAGER_H__
#define __ORG_FREEDESKTOP_DBUS_OBJECTMANAGER_H__


#include "simppl/interface.h"
#include "simppl/map.h"
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
                std::map<simppl::dbus::ObjectPath,
                    std::map<std::string,
                        std::map<std::string, simppl::dbus::Any>
                            >
                        > managed_objects_t;


            INTERFACE(ObjectManager)
            {
                Method<out<managed_objects_t>> GetManagedObjects;

                ObjectManager()
                 : INIT(GetManagedObjects)
                {
                    // NOOP
                }
            };
        }
    }
}


#endif   // __ORG_FREEDESKTOP_DBUS_OBJECTMANAGER_H__
