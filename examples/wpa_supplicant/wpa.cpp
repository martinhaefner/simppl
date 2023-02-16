#include <iostream>
#include <iomanip>

#include "simppl/dispatcher.h"
#include "simppl/stub.h"

#include "wpa_supplicant.h"


std::string get_description(const std::string& key_mgmt, int version)
{
    if (key_mgmt == "sae")
        return "WPA3-Personal";

    if (key_mgmt == "wpa-eap-suite-b-192")
        return "WPA3-EAP";

    if (key_mgmt == "wpa-psk")
        return version == 1 ? "WPA-PSK" : "WPA2-PSK";

    if (key_mgmt == "wpa-eap")
        return version == 1 ? "WPA-EAP" : "WPA2-EAP";

    if (key_mgmt == "wpa-psk-sha256")
        return "WPA2-PSK-SHA256";

    if (key_mgmt == "wpa-eap-sha256")
        return "WPA2-EAP-SHA256";

    return "unknown";
}


std::string to_string(const std::vector<uint8_t>& v)
{
    std::string rc;

    for(auto c : v)
    {
        if (c == 0)
        {
            rc += "\\00";
        }
        else
            rc.push_back(c);
    }

    return rc;
}


void eval(const simppl::dbus::Any& a, int version)
{
    if (a.is<std::string>())
    {
        std::cout << get_description(a.as<std::string>(), version) << " ";
    }
    else if (a.is<std::vector<std::string>>())
    {
        for(auto s: a.as<std::vector<std::string>>())
        {
            std::cout << get_description(s, version) << " ";
        }
    }
}


void parse_ies(const std::vector<uint8_t>& v)
{
    for(auto iter = v.begin(); iter != v.end();)
    {
        int type = *iter++;
        int len = *iter++;

        std::cout << std::hex << "        0x" << std::setw(2) << std::setfill('0') << type << ": ";

        for(int i=0; i<len; ++i)
        {
            std::cout << std::setw(2) << std::setfill('0') << (int)*iter++ << " ";

            if (i % 10 == 9 && i<len-1)
                std::cout << std::endl << "              ";
        }

        std::cout << std::dec << std::endl;
    }
}


int main()
{
    simppl::dbus::Dispatcher d("bus:system");

    // This is an assumption that may not be correct in your environment. As noted in the interface definition file,
    // calling the method GetInterface on fi::w1::wpa_supplicant1 in not that simple since C++ namespace and Interface definition
    // whould then be mixed up.
    simppl::dbus::Stub<fi::w1::wpa_supplicant1::Interface> supplicant(d, "fi.w1.wpa_supplicant1", "/fi/w1/wpa_supplicant1/Interfaces/0");

    // issue a scan
    std::map<std::string, simppl::dbus::Any> args;
    args["Type"] = std::string("active");

    try
    {
        supplicant.Scan(args);
    }
    catch(std::exception& ex)
    {
        std::cerr << "Scan failed: " << ex.what() << std::endl;
    }

    auto bsss = supplicant.BSSs.get();

    for (auto bss_path : bsss)
    {
        simppl::dbus::Stub<fi::w1::wpa_supplicant1::BSS> bss(d, "fi.w1.wpa_supplicant1", bss_path.path.c_str());

        std::cout << to_string(bss.SSID.get()) << "  at " << bss_path.path << std::endl;

        std::cout << "    KeyMgmt(s):" << std::endl << "        ";

        // WPA2+
        auto rsn = bss.RSN.get();
        eval(rsn["KeyMgmt"], 2);

        // WPA
        auto wpa = bss.WPA.get();
        eval(wpa["KeyMgmt"], 1);

        std::cout << std::endl;

        // information elements
        std::cout << "    IEs: " << std::endl;
        auto ies = bss.IEs.get();
        parse_ies(ies);

        std::cout << std::endl;
    }

    return 0;
}
