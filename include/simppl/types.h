#ifndef SIMPPL_TYPES_H
#define SIMPPL_TYPES_H


#include <memory>

#include <dbus/dbus.h>


namespace simppl
{

namespace dbus
{


typedef std::unique_ptr<::DBusMessage, void(*)(::DBusMessage*)> dbus_message_ptr_t;

// FIXME same for PendingCall


}   // namespace simppl

}   // namespace dbus


#endif   // SIMPPL_TYPES_H
