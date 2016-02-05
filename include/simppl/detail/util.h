#ifndef SIMPPL_DETAIL_UTIL_H
#define SIMPPL_DETAIL_UTIL_H


#include <tuple>


namespace simppl
{
   
namespace ipc
{

namespace detail
{

/**
 * @return a tuple with <0> a pointer to the objectpath which has to be deleted[]
 *         when unused any more and <1> a pointer to the rolename which points
 *         to the rolename within the objectpath.
 */
std::tuple<char*, char*> create_objectpath(const char* iface, const char* role);   


/**
 * @return dbus compatible interface name from mangled c++ name. Must be
 *         delete via delete[].
 */
char* extract_interface(const char* mangled_iface);

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_UTIL_H
