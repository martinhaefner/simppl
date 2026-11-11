#ifndef SIMPPL_DETAIL_INTERFACENAME_H
#define SIMPPL_DETAIL_INTERFACENAME_H


#include <string>
#include <typeinfo>

#include "simppl/interface.h"


namespace simppl
{

namespace dbus
{

namespace detail
{

/**
 * @return DBus interface name derived from the mangled name of an
 *         instantiated interface type, e.g. "org.freedesktop.DBus.ObjectManager"
 *         for org::freedesktop::DBus::ObjectManager<0, ...>.
 */
std::string interface_name_from_type(const char* mangled_type_name);


/**
 * @return the DBus interface name of interface @c IfaceT: the one given via
 *         @c INTERFACE(iface, name) or the one derived from the C++ name.
 *
 * @tparam InstanceT any instantiation of @c IfaceT, only used for its type name.
 */
template<template<int,
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class,
                  typename> class IfaceT,
         typename InstanceT>
inline
std::string resolve_interface_name()
{
   // unqualified call: ADL finds the overload generated next to a named interface
   constexpr const char* explicit_name = simppl_interface_name(static_cast<interface_tag<IfaceT>*>(nullptr));

   if constexpr (explicit_name != nullptr)
   {
      return explicit_name;
   }
   else
      return interface_name_from_type(typeid(InstanceT).name());
}

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_INTERFACENAME_H
