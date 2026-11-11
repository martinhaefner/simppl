#ifndef SIMPPL_STUB_H
#define SIMPPL_STUB_H


#include <algorithm>
#include <typeinfo>
#include <cxxabi.h>

#include "simppl/stubbase.h"
#include "simppl/clientside.h"
#include "simppl/typelist.h"

#include "simppl/detail/constants.h"
#include "simppl/detail/interfacename.h"


namespace simppl
{

namespace dbus
{


// forward decl
void dispatcher_add_stub(Dispatcher&, StubBase&);

// Client stubs do not require multiple interface support - you can simply
// create multiple stubs with different interfaces.
template<template<int,
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class,
                  typename> class IfaceT>
struct Stub : IfaceT<0, ClientMethod, ClientSignal, ClientProperty, StubBase>
{
   friend struct Dispatcher;

private:

   using instance_type = IfaceT<0, ClientMethod, ClientSignal, ClientProperty, StubBase>;

public:

   inline
   Stub(Dispatcher& disp, const char* role)
   {
       this->init(detail::resolve_interface_name<IfaceT, instance_type>(), role);
       dispatcher_add_stub(disp, *this);
   }


   inline
   Stub(Dispatcher& disp, const char* busname, const char* objectpath)
   {
      this->init(detail::resolve_interface_name<IfaceT, instance_type>(), busname, objectpath);
      dispatcher_add_stub(disp, *this);
   }
};

}   // namespace dbus

}   // namespace simppl


// ---------------------------------------------------------------------------------


/// for storing a function in the connected function object
template<typename CallableT>
inline
void operator>> (simppl::dbus::StubBase::Connected& conn, const CallableT& callable)
{
   conn.set_callback(callable);
}


#endif   // SIMPPL_STUB_H
