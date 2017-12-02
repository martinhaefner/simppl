#ifndef SIMPPL_STUB_H
#define SIMPPL_STUB_H


#include <algorithm>
#include <typeinfo>
#include <cxxabi.h>

#include "simppl/stubbase.h"
#include "simppl/clientside.h"

#include "simppl/detail/constants.h"


namespace simppl
{

namespace dbus
{


// forward decl
void dispatcher_add_stub(Dispatcher&, StubBase&);


template<template<template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class, typename> class IfaceT>
struct Stub : IfaceT<ClientMethod, ClientSignal, ClientProperty, StubBase>
{
   friend struct Dispatcher;

private:

   typedef IfaceT<ClientMethod, ClientSignal, ClientProperty, StubBase> interface_type;

public:

   inline
   Stub(Dispatcher& disp, const char* role)	
    : interface_type()
   {
	   this->init(abi::__cxa_demangle(typeid(interface_type).name(), 0, 0, 0), role);
       dispatcher_add_stub(disp, *this);
   }


   inline
   Stub(Dispatcher& disp, const char* busname, const char* objectpath)
    : interface_type()
   {
	  this->init(abi::__cxa_demangle(typeid(interface_type).name(), 0, 0, 0), busname, objectpath);
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
