#ifndef SIMPPL_SKELETON_H
#define SIMPPL_SKELETON_H


#include <typeinfo>

#include "simppl/skeletonbase.h"
#include "simppl/dispatcher.h"
#include "simppl/serverside.h"
#include "simppl/typelist.h"
#include "simppl/detail/interposer.h"


namespace simppl
{

namespace dbus
{

// forward decl
void dispatcher_add_skeleton(Dispatcher&, StubBase&);


template<template<int,
                  typename,
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class,
                  typename> class... Is>
struct Skeleton : detail::Interposer<sizeof...(Is)-1, detail::SizedSkeletonBase<sizeof...(Is)>, ServerMethod, ServerSignal, ServerProperty, Is...>
{
    friend struct Dispatcher;

    using interface_list = make_typelist<Is<0, SkeletonBase, ServerMethod, ServerSignal, ServerProperty, SkeletonBase>...>;

    static constexpr std::size_t iface_count = sizeof...(Is);

    inline
    Skeleton(Dispatcher& disp, const char* role)
    {
        static_assert(iface_count == 1, "Generating bus and object names from a role only works with a single interface");
        this->init(typeid(interface_list).name(), role);
        dispatcher_add_skeleton(disp, *this);
    }

    inline
    Skeleton(Dispatcher& disp, std::string busname, std::string objectpath)
    {
        this->init(iface_count, typeid(interface_list).name(), std::move(busname), std::move(objectpath));
        dispatcher_add_skeleton(disp, *this);
    }


protected:

    ServerRequestDescriptor current_request_;
};


template<>
struct Skeleton<> : detail::SizedSkeletonBase<0>
{
    static constexpr std::size_t iface_count = 0;

    Skeleton(Dispatcher& disp, std::string busname, std::string objectpath)
    {
        this->init(std::move(busname), std::move(objectpath));
        dispatcher_add_skeleton(disp, *this);
    }

protected:
    ServerRequestDescriptor current_request_;
};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_SKELETON_H
