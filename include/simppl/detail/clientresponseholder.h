#ifndef SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H
#define SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H


#include <cstdint>
#include <tuple>


// forward decl
struct ClientResponseBase;
struct DBusPendingCall;


namespace simppl
{

namespace dbus
{

// forward decl
struct Dispatcher;


namespace detail
{

struct ClientResponseHolder
{
   inline
   ClientResponseHolder(Dispatcher& disp, ClientResponseBase* r, uint32_t serial)
    : dispatcher_(disp)
    , r_(r)
    , serial_(serial)
   {
      // NOOP
   }

   Dispatcher& dispatcher_;
   ClientResponseBase* r_;
   uint32_t serial_;
};


} // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H
