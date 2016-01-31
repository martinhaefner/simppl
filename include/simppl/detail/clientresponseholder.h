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
   ClientResponseHolder(Dispatcher& disp, ClientResponseBase* r, DBusPendingCall* pending)
    : dispatcher_(disp)
    , r_(r)
    , pending_(pending)
   {
      // NOOP
   }
   
   /** 
    * FIXME still need this?
    * 
    * Blocking call semantic:
    *  
    * bool rc = (bool)stub.func(42);
    */
   explicit
   operator bool();

   Dispatcher& dispatcher_;
   ClientResponseBase* r_;
   DBusPendingCall* pending_;
};

} // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H
