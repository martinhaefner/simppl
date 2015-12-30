#ifndef SIMPPL_SKELETON_H
#define SIMPPL_SKELETON_H


#include "simppl/skeletonbase.h"
#include "simppl/dispatcher.h"


namespace simppl
{
   
namespace ipc
{

namespace detail
{
   template<typename> struct ServerHolder;
}


template<template<template<typename...> class, 
                  template<typename...> class,
                  template<typename...> class,
                  template<typename,typename> class> class IfaceT>
struct Skeleton : SkeletonBase, IfaceT<ServerRequest, ServerResponse, ServerSignal, ServerAttribute>
{
   friend struct Dispatcher;
   template<typename> friend struct detail::ServerHolder;
   
   typedef IfaceT<ServerRequest, ServerResponse, ServerSignal, ServerAttribute> interface_type;
   
   inline
   Skeleton(const char* role)
    : SkeletonBase(role)
   {
      // NOOP
   }
   
   /*inline
   std::string fqn() const
   {
      return Dispatcher::fullQualifiedName(InterfaceNamer<interface_type>::name(), role_);
   }*/
   
protected:
   
   /*bool find(uint32_t funcid, std::map<uint32_t, ServerRequestBase*>::iterator& iter)
   {
      iter = ((interface_type*)this)->container_.find(funcid);
      return iter != ((interface_type*)this)->container_.end();
   }*/
   
   ServerRequestDescriptor current_request_;
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_SKELETON_H
