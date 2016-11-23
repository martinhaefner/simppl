#ifndef SIMPPL_DETAIL_UTIL_H
#define SIMPPL_DETAIL_UTIL_H


#include <tuple>


namespace simppl
{

namespace dbus
{

namespace detail
{

/**
 * Trivially mangle iface with role to create objectpath.
 */
char* create_objectpath(const char* iface, const char* role);

/**
 * Trivially mangle iface with role to create busname.
 */
char* create_busname(const char* iface, const char* role);


/**
 * @return dbus compatible interface name from mangled c++ name. Must be
 *         delete via delete[].
 */
char* extract_interface(const char* mangled_iface);

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_UTIL_H
