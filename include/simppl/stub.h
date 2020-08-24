#ifndef SIMPPL_STUB_H
#define SIMPPL_STUB_H


#include <algorithm>
#include <typeinfo>
#include <cxxabi.h>

#include "simppl/stubbase.h"
#include "simppl/clientside.h"
#include "simppl/typelist.h"

#include "simppl/detail/constants.h"


namespace simppl
{

namespace dbus
{


// forward decl
void dispatcher_add_stub(Dispatcher&, StubBase&);

// Client stubs do not require multiple interface support - you can simply
// create multiple stubs with different interfaces.
template<template<int,
                  typename,
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class,
                  typename> class IfaceT>
struct Stub : IfaceT<0, StubBase, ClientMethod, ClientSignal, ClientProperty, StubBase>
{
   friend struct Dispatcher;

private:

   using interface_list = make_typelist<IfaceT<0, StubBase, ClientMethod, ClientSignal, ClientProperty, StubBase>>;

public:

   inline
   Stub(Dispatcher& disp, const char* role)
   {
	   this->init(abi::__cxa_demangle(typeid(interface_list).name(), 0, 0, 0), role);
       dispatcher_add_stub(disp, *this);
   }


   inline
   Stub(Dispatcher& disp, const char* busname, const char* objectpath)
   {
	  this->init(abi::__cxa_demangle(typeid(interface_list).name(), 0, 0, 0), busname, objectpath);
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
