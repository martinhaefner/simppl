#ifndef __FI_W1_WPA_SUPPLICANT_H__
#define __FI_W1_WPA_SUPPLICANT_H__


// interface definition
#include "simppl/interface.h"

// used data types
#include "simppl/objectpath.h"
#include "simppl/vector.h"
#include "simppl/map.h"
#include "simppl/any.h"
#include "simppl/string.h"


/**
 * @note wpa_supplicant unfortunately defines interfaces as part of other interfaces.
 * In this example we omit the wpa_supplicant1 interface. To implement this, the
 * interfaces may be modelled in different compilation units. For now, there is
 * no other solution.
 */
namespace fi
{

namespace w1
{

namespace wpa_supplicant1
{

INTERFACE(Interface)
{
    Method<simppl::dbus::in<std::map<std::string, simppl::dbus::Any>>> Scan;

    Property<std::vector<simppl::dbus::ObjectPath>> BSSs;

    Interface()
     : INIT(Scan)
     , INIT(BSSs)
    {
        // NOOP
    }
};


INTERFACE(BSS)
{
    Property<std::vector<uint8_t>> SSID;
    Property<std::vector<uint8_t>> IEs;

    // WPA2-xxx
    Property<std::map<std::string, simppl::dbus::Any>> RSN;

    // WPA-xxx
    Property<std::map<std::string, simppl::dbus::Any>> WPA;


    BSS()
     : INIT(SSID)
     , INIT(IEs)
     , INIT(RSN)
     , INIT(WPA)
    {
        // NOOP
    }
};

}   // namespace

}   // namespace

}   // namespace


#endif   // __FI_W1_WPA_SUPPLICANT_H__
