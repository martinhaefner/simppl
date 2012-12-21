#ifndef SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H
#define SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H


#include <cstdint>


// forward decl
struct ClientResponseBase;


namespace detail
{

struct ClientResponseHolder
{
   inline
   ClientResponseHolder(ClientResponseBase* r, uint32_t sequence_nr)
    : r_(r)
    , sequence_nr_(sequence_nr)
   {
      // NOOP
   }
   
   ClientResponseBase* r_;
   uint32_t sequence_nr_;
};

} // namespace detail


#endif   // SIMPPL_DETAIL_CLIENTRESPONSEHOLDER_H