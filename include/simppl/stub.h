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
   
namespace ipc
{

// forward decl
template<typename> struct InterfaceNamer;


template<template<template<typename...> class, 
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,typename> class> 
   class IfaceT>
struct Stub : StubBase, IfaceT<ClientRequest, ClientResponse, ClientSignal, ClientAttribute>
{   
   friend struct Dispatcher;
   
private:

   typedef IfaceT<ClientRequest, ClientResponse, ClientSignal, ClientAttribute> interface_type;
   
public:
   
   Stub(const char* role)
    : StubBase(abi::__cxa_demangle(typeid(interface_type).name(), 0, 0, 0)/*InterfaceNamer<interface_type>::name()*/, role)
   {
      // NOOP
   }
};

}   // namespace ipc

}   // namespace simppl


// ---------------------------------------------------------------------------------


/// for storing a function in the connected function object
template<typename CallableT>
inline
void operator>> (std::function<void(simppl::ipc::ConnectionState)>& func, const CallableT& callable)
{
   func = callable;
}


#endif   // SIMPPL_STUB_H
