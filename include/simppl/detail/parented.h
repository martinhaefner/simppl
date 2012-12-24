#ifndef SIMPPL_DETAIL_PARENTED_H
#define SIMPPL_DETAIL_PARENTED_H


#include <cassert>

// forward decls 
struct StubBase;
struct Dispatcher;
   

namespace simppl
{
   
namespace ipc
{
   
struct StubBase;
struct Dispatcher;

namespace detail
{

struct Parented
{
   friend struct simppl::ipc::StubBase;
   friend struct simppl::ipc::Dispatcher;
   
protected:
 
   inline
   Parented()
    : parent_(0)
   {
      // NOOP
   }
      
   inline
   void reparent(void* parent)
   {
      assert(parent_);
      parent_ = parent;
   }
   
   template<typename ParentT>
   inline
   ParentT* parent()
   {
      assert(parent_);
      return (ParentT*)parent_;
   }
   
   inline
   ~Parented()
   {
      // NOOP
   }
   
   void* parent_;
};

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_PARENTED_H