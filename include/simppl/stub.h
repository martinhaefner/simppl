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
      std::for_each(((interface_type*)this)->container_.begin(), ((interface_type*)this)->container_.end(), std::bind(&StubBase::reparent, this, std::placeholders::_1));
      ((interface_type*)this)->container_.clear();
   }
   
   /// blocking connect to server
   bool connect()
   {
      assert(!dispatcherIsRunning());
      
      bool rc = true;
      
      if (fd_ <= 0)
         rc = StubBase::connect(true);   // inherited friendship
      
      return rc;
   }
};

}   // namespace ipc

}   // namespace simppl


// ---------------------------------------------------------------------------------


/// for storing a function in the connected function object
template<typename CallableT>
inline
void operator>> (std::function<void()>& func, const CallableT& callable)
{
   func = callable;
}


#endif   // SIMPPL_STUB_H