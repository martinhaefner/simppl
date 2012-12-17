#ifndef SIMPPL_INTERFACE_H
#define SIMPPL_INTERFACE_H


#include <cstdint>
#include <vector>
#include <map>

#include "simppl/clientside.h"
#include "simppl/serverside.h"


struct AbsoluteInterfaceBase
{      
   inline
   uint32_t nextId()
   {
      return ++id_;
   }
   
protected:

   inline
   AbsoluteInterfaceBase()
    : id_(0)
   {
      // NOOP
   }

   inline
   ~AbsoluteInterfaceBase()
   {
      // NOOP
   }
   
   uint32_t id_;   // mutable variable for request/signals id registration, after startup this var has an senseless value
};


template<template<typename...> class RequestT>
struct InterfaceBase;


template<>
struct InterfaceBase<ClientRequest> : AbsoluteInterfaceBase
{
   inline
   InterfaceBase()
    : signals_(container_)
   {
      // NOOP
   }
   
   // temporary lists
   std::vector<Parented*> container_;
   std::vector<Parented*>& signals_;
};


template<>
struct InterfaceBase<ServerRequest> : AbsoluteInterfaceBase
{
   std::map<uint32_t, ServerRequestBase*> container_;
   std::map<uint32_t, ServerSignalBase*> signals_;
};


#define INTERFACE(iface) \
   template<template<typename...> class Request, \
            template<typename...> class Response, \
            template<typename...> class Signal, \
            template<typename,typename=OnChange> class Attribute> \
      struct iface; \
            \
   template<> struct InterfaceNamer<iface<ClientRequest, ClientResponse, ClientSignal, ClientAttribute> > { static inline const char* const name() { return #  iface ; } }; \
   template<> struct InterfaceNamer<iface<ServerRequest, ServerResponse, ServerSignal, ServerAttribute> > { static inline const char* const name() { return #  iface ; } }; \
   template<template<typename...> class Request, \
            template<typename...> class Response, \
            template<typename...> class Signal, \
            template<typename,typename=OnChange> class Attribute> \
      struct iface : InterfaceBase<Request>

#define INIT_REQUEST(request) \
   request(((AbsoluteInterfaceBase*)this)->nextId(), ((InterfaceBase<Request>*)this)->container_)

#define INIT_RESPONSE(response) \
   response()

#define INIT_SIGNAL(signal) \
   signal(((AbsoluteInterfaceBase*)this)->nextId(), ((InterfaceBase<Request>*)this)->signals_)

// an attribute is nothing more that an encapsulated signal
#define INIT_ATTRIBUTE(attr) \
   attr(((AbsoluteInterfaceBase*)this)->nextId(), ((InterfaceBase<Request>*)this)->signals_)


template<typename... T, typename... T2>
inline
void operator>> (ClientRequest<T...>& request, ClientResponse<T2...>& response)
{
   assert(!request.handler_);
   request.handler_ = &response;
}


template<typename... T, typename... T2>
inline
void operator>> (ServerRequest<T...>& request, ServerResponse<T2...>& response)
{
   assert(!request.hasResponse());
   
   ServerRequestBaseSetter::setHasResponse(request);
   response.allowedRequests_.insert(&request);   
}

#endif   // SIMPPL_INTERFACE_H
