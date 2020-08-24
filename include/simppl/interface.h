#ifndef SIMPPL_INTERFACE_H
#define SIMPPL_INTERFACE_H


#include "simppl/serialization.h"
#include "simppl/parameter_deduction.h"
#include "simppl/property.h"


#define INTERFACE(iface) \
   template<int InterfaceID, \
            typename AncestorT, \
            template<typename...> class Method, \
            template<typename...> class Signal, \
            template<typename, int Flags=simppl::dbus::Notifying|simppl::dbus::ReadOnly|simppl::dbus::OnChange> class Property, \
            typename BaseT> \
      struct iface : BaseT

#define INIT(what) \
   what(# what, this, InterfaceID)


#endif   // SIMPPL_INTERFACE_H
