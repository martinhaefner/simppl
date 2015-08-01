#ifndef SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H
#define SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H


#include <cstdint>
#include <tuple>


// forward decl
struct ClientResponseBase;


namespace simppl
{
   
namespace ipc
{

// forward decl
struct Dispatcher;

   
namespace detail
{

struct ClientResponseHolder
{
   inline
   ClientResponseHolder(ClientResponseBase* r, uint32_t sequence_nr, simppl::ipc::Dispatcher& dispatcher)
    : r_(r)
    , sequence_nr_(sequence_nr)
    , dispatcher_(dispatcher)
   {
      // NOOP
   }
   
   /** 
    * Blocking call semantic:
    *  
    * bool rc = (bool)stub.func(42);
    */
   explicit
   operator bool();

   ClientResponseBase* r_;
   uint32_t sequence_nr_;
   
   simppl::ipc::Dispatcher& dispatcher_;
};

} // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H
