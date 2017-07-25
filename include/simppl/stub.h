#ifndef SIMPPL_STUB_H
#define SIMPPL_STUB_H


#include <algorithm>
#include <typeinfo>
#include <cxxabi.h>

#include "simppl/stubbase.h"
#include "simppl/clientside.h"

#include "simppl/detail/parented.h"
#include "simppl/detail/constants.h"


namespace simppl
{

namespace dbus
{


// forward decl
void dispatcher_add_stub(Dispatcher&, StubBase&);


template<template<template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class>
   class IfaceT>
struct Stub : StubBase, IfaceT<ClientRequest, ClientSignal, ClientProperty>
{
   friend struct Dispatcher;

private:

   typedef IfaceT<ClientRequest, ClientSignal, ClientProperty> interface_type;

public:

   inline
   Stub(Dispatcher& disp, const char* role)
    : StubBase(abi::__cxa_demangle(typeid(interface_type).name(), 0, 0, 0), role)
   {
       dispatcher_add_stub(disp, *this);
   }


   inline
   Stub(Dispatcher& disp, const char* busname, const char* objectpath)
    : StubBase(abi::__cxa_demangle(typeid(interface_type).name(), 0, 0, 0), busname, objectpath)
   {
      dispatcher_add_stub(disp, *this);
   }
};

}   // namespace dbus

}   // namespace simppl


// ---------------------------------------------------------------------------------


/// for storing a function in the connected function object
template<typename CallableT>
inline
void operator>> (std::function<void(simppl::dbus::ConnectionState)>& func, const CallableT& callable)
{
   func = callable;
}


#endif   // SIMPPL_STUB_H
