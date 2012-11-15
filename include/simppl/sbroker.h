#ifndef SIMPPL_SBROKER_H
#define SIMPPL_SBROKER_H


#include "simppl/ipc2.h"


struct ServiceInfo
{
   typedef make_serializer<std::string, std::string>::type serializer_type;
   
   inline
   ServiceInfo()
   {
      // NOOP
   }
   
   inline
   ServiceInfo(const std::string& name, const std::string& loc)
    : name_(name)
    , location_(loc)
   {
      // NOOP
   }
   
   std::string name_;
   std::string location_;
};


/// servicebroker
INTERFACE(Broker)
{   
   Request<std::string/*servicename*/> waitForService;
   
   /// FIXME must be a vector of locations
   Request<std::string/*servicename*/, std::string/*location*/> registerService;
   Request<std::vector<ServiceInfo> > registerServices;
   
   Request<std::string/*servicename*/> unregisterService;
   Request<std::vector<std::string/*servicename*/> > unregisterServices;
   
   Request<> listServices;
   Request<> listWaiters;
   
   Response<std::string/*servicename*/, std::string/*location*/> serviceReady;
   Response<std::vector<ServiceInfo> > serviceList;
   Response<std::vector<std::string> > waitersList;
   
   inline
   Broker()
    : INIT_REQUEST(waitForService)
    , INIT_REQUEST(registerService)
    , INIT_REQUEST(registerServices)
    , INIT_REQUEST(unregisterService)
    , INIT_REQUEST(unregisterServices)
    , INIT_REQUEST(listServices)
    , INIT_REQUEST(listWaiters)
    , INIT_RESPONSE(serviceReady)
    , INIT_RESPONSE(serviceList)
    , INIT_RESPONSE(waitersList)
   {
      waitForService >> serviceReady;
      listServices >> serviceList;
      listWaiters >> waitersList;
   }
};


#endif   // SIMPPL_SBROKER_H
