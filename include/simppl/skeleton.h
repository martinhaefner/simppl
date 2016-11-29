#ifndef SIMPPL_SKELETON_H
#define SIMPPL_SKELETON_H


#include <typeinfo>
#include <cxxabi.h>

#include "simppl/skeletonbase.h"
#include "simppl/dispatcher.h"
#include "simppl/serverside.h"


namespace simppl
{

namespace dbus
{

// forward decl
void dispatcher_add_skeleton(Dispatcher&, StubBase&);


template<template<template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class> class IfaceT>
struct Skeleton : SkeletonBase, IfaceT<ServerRequest, ServerSignal, ServerAttribute>
{
   friend struct Dispatcher;

   typedef IfaceT<ServerRequest, ServerSignal, ServerAttribute> interface_type;

   inline
   Skeleton(Dispatcher& disp, const char* role)
    : SkeletonBase(abi::__cxa_demangle(typeid(interface_type).name(), 0, 0, 0), role)
   {
      dispatcher_add_skeleton(disp, *this);
   }


protected:

   ServerRequestDescriptor current_request_;
};

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_SKELETON_H
