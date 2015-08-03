#include "simppl/detail/clientresponseholder.h"
#include "simppl/dispatcher.h"


simppl::ipc::detail::ClientResponseHolder::operator bool()
{
   dispatcher_.waitForResponse(*this);
   return true;   // FIXME remove the bool return value
}
