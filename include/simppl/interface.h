#ifndef SIMPPL_INTERFACE_H
#define SIMPPL_INTERFACE_H


#include <cstdint>
#include <vector>
#include <map>

#include "simppl/detail/parented.h"
#include "simppl/detail/basicinterface.h"
#include "simppl/detail/serversignalbase.h"  // FIXME can be removed probably...
#include "simppl/detail/serverrequestbasesetter.h"


namespace simppl
{
   
namespace ipc
{
   
// forward decls
template<typename...> struct ClientRequest;
template<typename...> struct ClientResponse;
template<typename...> struct ClientSignal;
template<typename, typename> struct ClientAttribute;

template<typename...> struct ServerRequest;
template<typename...> struct ServerResponse;
template<typename...> struct ServerSignal;
template<typename, typename> struct ServerAttribute;

struct ServerRequestBase;
struct ServerAttributeBase;
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
};

}   // namespace ipc

}   // namespace simppl


/// make sure to NOT put this macro into a namespace
#define INTERFACE(iface) \
   template<template<typename...> class Request, \
            template<typename...> class Response, \
            template<typename...> class Signal, \
            template<typename,typename=simppl::ipc::OnChange> class Attribute> \
      struct iface : public simppl::ipc::InterfaceBase<Request>

#define INIT_REQUEST(request) \
   request(# request, this)

#define INIT_RESPONSE(response) \
   response(this)

#define INIT_SIGNAL(signal) \
   signal(# signal, this)

// an attribute is nothing more that an encapsulated signal
#define INIT_ATTRIBUTE(attr) \
   attr(# attr, this)


template<typename... T, typename... T2>
inline
void operator>> (simppl::ipc::ClientRequest<T...>& request, simppl::ipc::ClientResponse<T2...>& response)
{
   assert(!request.handler_);
   request.handler_ = &response;
}


template<typename... T, typename... T2>
inline
void operator>> (simppl::ipc::ServerRequest<T...>& request, simppl::ipc::ServerResponse<T2...>& response)
{
   assert(!request.hasResponse());
   
   simppl::ipc::detail::ServerRequestBaseSetter::setHasResponse(request);
   response.allowedRequests_.insert(&request);   
}

#endif   // SIMPPL_INTERFACE_H
