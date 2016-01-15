#ifndef SIMPPL_DETAIL_SERVERSIGNALBASE_H
#define SIMPPL_DETAIL_SERVERSIGNALBASE_H


#include "simppl/detail/signalrecipient.h"
#include "simppl/detail/serialization.h"


namespace simppl
{

namespace ipc
{

namespace detail
{

struct ServerRequestBase
{
};


// FIXME obsolete?
struct ServerSignalBase
{
protected:

   ~ServerSignalBase();
};

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERVERSIGNALBASE_H
