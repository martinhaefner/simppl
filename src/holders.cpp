#include "simppl/pendingcall.h"
#include "simppl/stub.h"

#include "simppl/detail/holders.h"


simppl::dbus::detail::GetAllPropertiesHolder::GetAllPropertiesHolder(std::function<void(const CallState&)> f, StubBase& stub)
 : f_(f)
 , stub_(stub)
{
  // NOOP
}


/*static*/
void simppl::dbus::detail::GetAllPropertiesHolder::_delete(void* p)
{
  auto that = (GetAllPropertiesHolder*)p;
  delete that;
}


/*static*/
void simppl::dbus::detail::GetAllPropertiesHolder::pending_notify(DBusPendingCall* pc, void* data)
{
  auto msg = simppl::dbus::make_message(dbus_pending_call_steal_reply(pc));

   auto that = (GetAllPropertiesHolder*)data;
   assert(that->f_);

   auto cs = that->stub_.get_all_properties_handle_response(*msg, false);
   that->f_(cs);
}
