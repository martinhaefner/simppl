#include "simppl/detail/clientresponseholder.h"
#include "simppl/dispatcher.h"


simppl::ipc::detail::ClientResponseHolder::operator bool()
{
   return dispatcher_.waitForResponse(*this);
}
