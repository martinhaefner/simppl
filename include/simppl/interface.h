#ifndef SIMPPL_INTERFACE_H
#define SIMPPL_INTERFACE_H


#include <cstdint>

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


}   // namespace dbus

}   // namespace simppl


#define INTERFACE(iface) \
   template<template<typename...> class Request, \
            template<typename...> class Signal, \
            template<typename, int Flags=simppl::dbus::Notifying|simppl::dbus::ReadOnly> class Property, \
            typename BaseT> \
      struct iface : public BaseT

#define INIT(what) \
   what(# what, this)


#endif   // SIMPPL_INTERFACE_H
