#ifndef SIMPPL_API_DBUS_H
#define SIMPPL_API_DBUS_H


#include <cstdint>

#include "simppl/interface.h"

#include "simppl/string.h"
#include "simppl/vector.h"
#include "simppl/map.h"
#include "simppl/any.h"
#include "simppl/bool.h"


namespace org
{

namespace freedesktop
{

using simppl::dbus::in;
using simppl::dbus::out;


/**
 * The message bus driver interface "org.freedesktop.DBus", see
 * https://dbus.freedesktop.org/doc/dbus-specification.html#message-bus-messages
 *
 * Complete as of dbus-daemon 1.16 (bus/driver.c). The driver is reachable
 * as busname "org.freedesktop.DBus", objectpath "/org/freedesktop/DBus".
 * Watching services coming and going:
 *
 * @code
 * simppl::dbus::Stub<org::freedesktop::Bus> bus(disp, "org.freedesktop.DBus", "/org/freedesktop/DBus");
 *
 * bus.NameOwnerChanged.attach() >> [](const std::string& name, const std::string& old_owner, const std::string& new_owner){
 *    ...
 * };
 * @endcode
 *
 * The dispatcher's own service tracking always runs before any user
 * callback of NameOwnerChanged, so stubs created from within that callback
 * already see the new service.
 *
 * The C++ name differs from the DBus name since org::freedesktop::DBus
 * is already taken as namespace (e.g. by org::freedesktop::DBus::ObjectManager).
 *
 * Flag and reply constants for RequestName, ReleaseName and
 * StartServiceByName are the DBUS_NAME_FLAG_*, DBUS_REQUEST_NAME_REPLY_*,
 * DBUS_RELEASE_NAME_REPLY_* and DBUS_START_REPLY_* macros from
 * <dbus/dbus-shared.h>.
 */
INTERFACE(Bus, "org.freedesktop.DBus")
{
    // --- methods ---

    Method<out<std::string>>                                     Hello;
    Method<in<std::string>, in<uint32_t>, out<uint32_t>>         RequestName;
    Method<in<std::string>, out<uint32_t>>                       ReleaseName;
    Method<in<std::string>, in<uint32_t>, out<uint32_t>>         StartServiceByName;
    Method<in<std::map<std::string, std::string>>>               UpdateActivationEnvironment;
    Method<in<std::string>, out<bool>>                           NameHasOwner;
    Method<out<std::vector<std::string>>>                        ListNames;
    Method<out<std::vector<std::string>>>                        ListActivatableNames;
    Method<in<std::string>>                                      AddMatch;
    Method<in<std::string>>                                      RemoveMatch;
    Method<in<std::string>, out<std::string>>                    GetNameOwner;
    Method<in<std::string>, out<std::vector<std::string>>>       ListQueuedOwners;
    Method<in<std::string>, out<uint32_t>>                       GetConnectionUnixUser;
    Method<in<std::string>, out<uint32_t>>                       GetConnectionUnixProcessID;
    Method<in<std::string>, out<std::vector<uint8_t>>>           GetAdtAuditSessionData;
    Method<in<std::string>, out<std::vector<uint8_t>>>           GetConnectionSELinuxSecurityContext;
    Method<>                                                     ReloadConfig;
    Method<out<std::string>>                                     GetId;
    Method<in<std::string>,
           out<std::map<std::string, simppl::dbus::Any>>>        GetConnectionCredentials;

    // --- signals ---

    /**
     * (name, old_owner, new_owner): empty old_owner - name appeared,
     * empty new_owner - name vanished. Emitted for well-known and
     * unique (":1.42") names.
     */
    Signal<std::string, std::string, std::string>                NameOwnerChanged;

    /// unicast to the connection that lost/acquired the name
    Signal<std::string>                                          NameLost;
    Signal<std::string>                                          NameAcquired;

    Signal<>                                                     ActivatableServicesChanged;

    // --- properties (constant, never signalled via PropertiesChanged) ---

    Property<std::vector<std::string>, simppl::dbus::ReadOnly>   Features;
    Property<std::vector<std::string>, simppl::dbus::ReadOnly>   Interfaces;

    Bus()
     : INIT(Hello)
     , INIT(RequestName)
     , INIT(ReleaseName)
     , INIT(StartServiceByName)
     , INIT(UpdateActivationEnvironment)
     , INIT(NameHasOwner)
     , INIT(ListNames)
     , INIT(ListActivatableNames)
     , INIT(AddMatch)
     , INIT(RemoveMatch)
     , INIT(GetNameOwner)
     , INIT(ListQueuedOwners)
     , INIT(GetConnectionUnixUser)
     , INIT(GetConnectionUnixProcessID)
     , INIT(GetAdtAuditSessionData)
     , INIT(GetConnectionSELinuxSecurityContext)
     , INIT(ReloadConfig)
     , INIT(GetId)
     , INIT(GetConnectionCredentials)
     , INIT(NameOwnerChanged)
     , INIT(NameLost)
     , INIT(NameAcquired)
     , INIT(ActivatableServicesChanged)
     , INIT(Features)
     , INIT(Interfaces)
    {
        // NOOP
    }
};

}   // namespace freedesktop

}   // namespace org



#endif   // SIMPPL_API_DBUS_H
