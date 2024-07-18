#ifndef __SIMPPL_DBUS_OBJECTMANAGERINTERPOSER_H__
#define __SIMPPL_DBUS_OBJECTMANAGERINTERPOSER_H__

#if SIMPPL_HAVE_OBJECTMANAGER
#   include "simppl/api/objectmanager.h"
#   include "simppl/objectmanagermixin.h"

#   include "simppl/serverside.h"   // FIXME remove this?


namespace simppl
{

namespace dbus
{

namespace detail
{

/**
 * Check if object manager interface is part of the implemented interfaces.
 */
template<template<int,
                  typename,
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class,
                  typename> class... Is>
struct ObjectManagerInterposer
{
    enum { avail = Find<org::freedesktop::DBus::ObjectManager<0, SkeletonBase, ServerMethod, ServerSignal, ServerProperty, SkeletonBase>,
        typename make_typelist<Is<0, SkeletonBase, ServerMethod, ServerSignal, ServerProperty, SkeletonBase>...>::type>::value != -1 };

    typedef typename std::conditional<avail, SizedObjectManagerMixin<sizeof...(Is)>, SizedSkeletonBase<sizeof...(Is)>>::type type;
};

}   // detail

}   // dbus

}   // simppl

#else

namespace simppl
{

namespace dbus
{

namespace detail
{

/**
 * No objectmanager interposing. You may implement the API for yourself.
 */
template<template<int,
                  typename,
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,int> class,
                  typename> class... Is>
struct ObjectManagerInterposer
{
    typedef SizedSkeletonBase<sizeof...(Is)> type;
};

}   // detail

}   // dbus

}   // simppl

#endif   // SIMPPL_HAVE_OBJECTMANAGER

#endif   // __SIMPPL_DBUS_OBJECTMANAGERINTERPOSER_H__

