#ifndef SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H
#define SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H


#include <cstdint>
#include <tuple>


// forward decl
struct ClientResponseBase;
struct DBusPendingCall;


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
   ClientResponseHolder(ClientResponseBase* r, DBusPendingCall* pending)
    : r_(r)
    , pending_(pending)
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
   DBusPendingCall* pending_;
};

} // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H
