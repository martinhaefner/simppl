#ifndef SIMPPL_INTERFACE_H
#define SIMPPL_INTERFACE_H


#include "simppl/serialization.h"
#include "simppl/parameter_deduction.h"
#include "simppl/property.h"


namespace simppl
{

namespace dbus
{

namespace detail
{

/// tag for looking up the explicit DBus name of an interface via ADL
template<template<int,
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class,
                  typename> class IfaceT>
struct interface_tag
{
};


/**
 * Fallback for interfaces without explicit DBus name: nullptr means the
 * name is derived from the C++ name. Interfaces declared with a name get
 * a non-template overload in their own namespace, see @c INTERFACE.
 */
template<typename TagT>
constexpr
const char* simppl_interface_name(TagT*)
{
   return nullptr;
}

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#define SIMPPL_INTERFACE_TEMPLATE_HEAD \
   template<int InterfaceID, \
            template<typename...> class Method, \
            template<typename...> class Signal, \
            template<typename, int Flags=simppl::dbus::Notifying|simppl::dbus::ReadOnly|simppl::dbus::OnChange> class Property, \
            typename BaseT>

#define SIMPPL_INTERFACE_UNNAMED(iface) \
   SIMPPL_INTERFACE_TEMPLATE_HEAD \
      struct iface : BaseT

#define SIMPPL_INTERFACE_NAMED(iface, name) \
   template<int, \
            template<typename...> class, \
            template<typename...> class, \
            template<typename, int> class, \
            typename> \
      struct iface; \
   \
   constexpr \
   const char* simppl_interface_name(simppl::dbus::detail::interface_tag<iface>*) \
   { \
      return name; \
   } \
   \
   SIMPPL_INTERFACE_TEMPLATE_HEAD \
      struct iface : BaseT

#define SIMPPL_INTERFACE_SELECT(_1, _2, macro, ...) macro

/**
 * Declare an interface.
 *
 * By default the DBus interface name is derived from the fully qualified
 * C++ name, e.g. @c org::freedesktop::DBus::ObjectManager becomes
 * "org.freedesktop.DBus.ObjectManager". Pass the DBus name explicitly
 * if it cannot be expressed that way, e.g. because the C++ name would clash
 * with a namespace of the same name:
 *
 * @code
 * namespace test {
 *    INTERFACE(Calculator) { ... };                             // "test.Calculator"
 * }
 *
 * namespace org { namespace freedesktop {
 *    INTERFACE(Bus, "org.freedesktop.DBus") { ... };            // "org.freedesktop.DBus"
 * } }
 * @endcode
 */
#define INTERFACE(...) \
   SIMPPL_INTERFACE_SELECT(__VA_ARGS__, SIMPPL_INTERFACE_NAMED, SIMPPL_INTERFACE_UNNAMED, )(__VA_ARGS__)

#define INIT(what) \
   what(# what, this, InterfaceID)


#endif   // SIMPPL_INTERFACE_H
