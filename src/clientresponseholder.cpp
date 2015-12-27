#include "simppl/detail/clientresponseholder.h"
#include "simppl/dispatcher.h"


simppl::ipc::detail::ClientResponseHolder::operator bool()
{
   dbus_pending_call_block(pending_);
   return true;
}
