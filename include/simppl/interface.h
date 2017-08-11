#ifndef SIMPPL_INTERFACE_H
#define SIMPPL_INTERFACE_H


#include <cstdint>

#include "simppl/detail/basicinterface.h"

#include "simppl/serialization.h"
#include "simppl/parameter_deduction.h"
#include "simppl/variant.h"
#include "simppl/objectpath.h"
#include "simppl/property.h"


namespace simppl
{

namespace dbus
{

// forward decls
template<typename...> struct ClientRequest;
template<typename...> struct ClientSignal;
template<typename, int> struct ClientProperty;

template<typename...> struct ServerRequest;
template<typename...> struct ServerSignal;
template<typename, int> struct ServerProperty;

struct ServerRequestBase;
struct ServerPropertyBase;
struct ServerSignalBase;


template<template<typename...> class RequestT>
struct InterfaceBase;


template<>
struct InterfaceBase<ClientRequest> : detail::BasicInterface
{
};


template<>
struct InterfaceBase<ServerRequest> : detail::BasicInterface
{
   InterfaceBase()
    : methods_(nullptr)
    , properties_(nullptr)
#if SIMPPL_HAVE_INTROSPECTION
    , signals_(nullptr)
#endif
   {
      // NOOP
   }
   
   // linked list heads
   ServerRequestBase* methods_;    
   ServerPropertyBase* properties_;

#if SIMPPL_HAVE_INTROSPECTION
   ServerSignalBase* signals_;   
#endif
};

}   // namespace dbus

}   // namespace simppl


/// make sure to NOT put this macro into a namespace
#define INTERFACE(iface) \
   template<template<typename...> class Request, \
            template<typename...> class Signal, \
            template<typename, int Flags=simppl::dbus::Notifying|simppl::dbus::ReadOnly> class Property> \
      struct iface : public simppl::dbus::InterfaceBase<Request>

#define INIT(what) \
   what(# what, this)


#endif   // SIMPPL_INTERFACE_H
