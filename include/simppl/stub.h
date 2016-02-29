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
template<typename> struct InterfaceNamer;


template<template<template<typename...> class, 
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class> 
   class IfaceT>
struct Stub : StubBase, IfaceT<ClientRequest, ClientResponse, ClientSignal, ClientAttribute>
{   
   friend struct Dispatcher;
   
private:

   typedef IfaceT<ClientRequest, ClientResponse, ClientSignal, ClientAttribute> interface_type;
   
public:
   
   /**
    * @param unused For compatibility reason to socket based simppl only. Unused on dbus.
    */
   Stub(const char* role, const char* unused = nullptr)
    : StubBase(abi::__cxa_demangle(typeid(interface_type).name(), 0, 0, 0)/*InterfaceNamer<interface_type>::name()*/, role)
   {
      (void)unused;   // make compiler happy
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
