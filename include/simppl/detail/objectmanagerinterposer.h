#ifndef __SIMPPL_DBUS_OBJECTMANAGERINTERPOSER_H__
#define __SIMPPL_DBUS_OBJECTMANAGERINTERPOSER_H__


#if SIMPPL_HAVE_OBJECTMANAGER
#   include "simppl/api/objectmanager.h"
#   include "simppl/objectmanagermixin.h"
#endif


namespace simppl
{

namespace dbus
{

namespace detail
{

template<template<int, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class I>
struct make_server_type
{
    typedef I<0, ServerMethod, ServerSignal, ServerProperty, SkeletonBase> type;
};


#if SIMPPL_HAVE_OBJECTMANAGER


/**
 * Check if object manager interface is part of the implemented interfaces
 * and potentially include the mixin in the inheritance hierarchy.
 */
template<template<int, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class... Is>
struct ObjectManagerInterposer
{
    typedef typename make_server_type<org::freedesktop::DBus::ObjectManager>::type objectmanager_server_type;

    enum { avail = Find<objectmanager_server_type, typename make_typelist<typename make_server_type<Is>::type...>::type>::value != -1 };

    typedef typename std::conditional<avail, SizedObjectManagerMixin<sizeof...(Is)>, SizedSkeletonBase<sizeof...(Is)>>::type type;
};

#else

/**
 * No objectmanager interposing. You may implement the API for yourself.
 */
template<template<int, template<typename...> class, template<typename...> class, template<typename, int> class, typename> class... Is>
struct ObjectManagerInterposer
{
    typedef SizedSkeletonBase<sizeof...(Is)> type;
};

#endif   // SIMPPL_HAVE_OBJECTMANAGER


}   // detail

}   // dbus

}   // simppl


#endif   // __SIMPPL_DBUS_OBJECTMANAGERINTERPOSER_H__

