#ifndef SIMPPL_SKELETON_H
#define SIMPPL_SKELETON_H


#include "simppl/skeletonbase.h"


template<template<template<typename...> class, 
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,typename> class> class IfaceT>
struct Skeleton : SkeletonBase, IfaceT<ServerRequest, ServerResponse, ServerSignal, ServerAttribute>
{
   friend struct Dispatcher;
   template<typename SkelT> friend struct ServerHolder;
   
   typedef IfaceT<ServerRequest, ServerResponse, ServerSignal, ServerAttribute> interface_type;
   
   inline
   Skeleton(const char* role)
    : SkeletonBase(role)
   {
      // NOOP
   }
   
protected:
   
   bool find(uint32_t funcid, std::map<uint32_t, ServerRequestBase*>::iterator& iter)
   {
      iter = ((interface_type*)this)->container_.find(funcid);
      return iter != ((interface_type*)this)->container_.end();
   }
   
   const char* role_;
   Dispatcher* disp_;
   ServerRequestDescriptor current_request_;
};


#endif   // SIMPPL_SKELETON_H
