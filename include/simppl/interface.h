#ifndef SIMPPL_INTERFACE_H
#define SIMPPL_INTERFACE_H


#include <cstdint>
#include <vector>
#include <map>

#include "simppl/attribute.h"
#include "simppl/detail/parented.h"
#include "simppl/detail/basicinterface.h"
#include "simppl/detail/serverrequestbasesetter.h"


namespace simppl
{

namespace dbus
{

// forward decls
template<typename...> struct ClientRequest;
template<typename...> struct ClientResponse;
template<typename...> struct ClientSignal;
template<typename, int> struct ClientAttribute;

template<typename...> struct ServerRequest;
template<typename...> struct ServerResponse;
template<typename...> struct ServerSignal;
template<typename, int> struct ServerAttribute;

struct ServerRequestBase;
struct ServerAttributeBase;
struct ServerSignalBase;
struct ServerRequestBaseSetter;


template<template<typename...> class RequestT>
struct InterfaceBase;


template<>
struct InterfaceBase<ClientRequest> : detail::BasicInterface
{
};


template<>
struct InterfaceBase<ServerRequest> : detail::BasicInterface
{
   // FIXME maybe use intrusive container here or take char* from static linkage?!
   std::map<std::string, ServerRequestBase*> methods_;
   std::map<std::string, ServerAttributeBase*> attributes_;

#if SIMPPL_HAVE_INTROSPECTION
   std::map<std::string, ServerSignalBase*> signals_;
#endif
};

}   // namespace dbus

}   // namespace simppl


/// make sure to NOT put this macro into a namespace
#define INTERFACE(iface) \
   template<template<typename...> class Request, \
            template<typename...> class Response, \
            template<typename...> class Signal, \
            template<typename, int Flags=::simppl::dbus::Notifying|::simppl::dbus::ReadOnly> class Attribute> \
      struct iface : public ::simppl::dbus::InterfaceBase<Request>

#define INIT(what) \
   what(# what, this)


template<typename... T, typename... T2>
inline
void operator>> (simppl::dbus::ClientRequest<T...>& request, simppl::dbus::ClientResponse<T2...>& response)
{
   assert(!request.handler_);
   request.handler_ = &response;
}


template<typename... T, typename... T2>
inline
void operator>> (simppl::dbus::ServerRequest<T...>& request, simppl::dbus::ServerResponse<T2...>& response)
{
   assert(!request.hasResponse());

   simppl::dbus::detail::ServerRequestBaseSetter::setHasResponse(request);
   response.allowedRequests_.insert(&request);
}

#endif   // SIMPPL_INTERFACE_H
