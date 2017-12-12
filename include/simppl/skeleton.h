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
                  template<typename,int> class, typename> class IfaceT>
struct Skeleton : IfaceT<ServerMethod, ServerSignal, ServerProperty, SkeletonBase>
{
    friend struct Dispatcher;

    typedef IfaceT<ServerMethod, ServerSignal, ServerProperty, SkeletonBase> interface_type;

    inline
    Skeleton(Dispatcher& disp, const char* role)
     : interface_type()
    {
        this->init(abi::__cxa_demangle(typeid(interface_type).name(), 0, 0, 0), role);
        dispatcher_add_skeleton(disp, *this);
    }

    inline
    Skeleton(Dispatcher& disp, const char* busname, const char* objectpath)
     : interface_type()
    {
        this->init(abi::__cxa_demangle(typeid(interface_type).name(), 0, 0, 0), busname, objectpath);
        dispatcher_add_skeleton(disp, *this);
    }


protected:

    ServerRequestDescriptor current_request_;
};

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_SKELETON_H
