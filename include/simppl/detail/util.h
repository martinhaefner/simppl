#ifndef SIMPPL_DETAIL_UTIL_H
#define SIMPPL_DETAIL_UTIL_H

#include <string>
#include <vector>

namespace simppl
{

namespace dbus
{

namespace detail
{


/**
 * Trivially mangle iface with role to create objectpath.
 * Must be deleted via delete[].
 */
char* create_objectpath(const char* iface, const char* role);

/**
 * Trivially mangle iface with role to create busname.
 * Must be deleted via delete[].
 */
char* create_busname(const char* iface, const char* role);

/**
 * @return dbus compatible interface names from mangled c++ name.
 */
std::vector<std::string> extract_interfaces(std::size_t iface_count, const char* mangled_iface_list);

/**
 * @return dbus compatible interface name from mangled c++ name.
 */
std::string make_interface_name(const char* begin, const char* end);

/**
 * @return a pointer to the N'th interface name given a pointer to the template
 * args of interface N-1 or `nullptr` if there are no more interface names.
 */
const char* find_next_interface(const char* template_args);



}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_UTIL_H
