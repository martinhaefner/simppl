#ifndef SIMPPL_STUB_H
#define SIMPPL_STUB_H


#include <algorithm>

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
   
   Stub(const char* role, const char* boundname)
    : StubBase(InterfaceNamer<interface_type>::name(), role, boundname)
   {
      std::for_each(((interface_type*)this)->container_.begin(), ((interface_type*)this)->container_.end(), std::bind(&Stub<IfaceT>::reparent, this, std::placeholders::_1));
      ((interface_type*)this)->container_.clear();
   }
   
   /// Blocking connect to server.
   /// Note that even in blocking mode the stub needs to be added 
   /// to the dispatcher.
   void connect()
   {
      StubBase::connect();
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
