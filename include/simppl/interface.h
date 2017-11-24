#ifndef SIMPPL_INTERFACE_H
#define SIMPPL_INTERFACE_H


#include "simppl/serialization.h"
#include "simppl/parameter_deduction.h"
#include "simppl/property.h"


#define INTERFACE(iface) \
   template<template<typename...> class Request, \
            template<typename...> class Signal, \
            template<typename, int Flags=simppl::dbus::Notifying|simppl::dbus::ReadOnly> class Property, \
            typename BaseT> \
      struct iface : public BaseT

#define INIT(what) \
   what(# what, this)


#endif   // SIMPPL_INTERFACE_H
