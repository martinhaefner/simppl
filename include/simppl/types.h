#ifndef SIMPPL_TYPES_H
#define SIMPPL_TYPES_H


#include <memory>

#include <dbus/dbus.h>


namespace simppl
{

namespace dbus
{


typedef std::unique_ptr<::DBusMessage, void(*)(::DBusMessage*)> dbus_message_ptr_t;

inline
dbus_message_ptr_t make_message(DBusMessage* p)
{
    return dbus_message_ptr_t(p, &dbus_message_unref);
}


typedef std::unique_ptr<DBusPendingCall, void(*)(DBusPendingCall*)> dbus_pending_call_ptr_t;

inline
dbus_pending_call_ptr_t make_pending_call(DBusPendingCall* p)
{
    return dbus_pending_call_ptr_t(p, &dbus_pending_call_unref);
}


}   // namespace simppl

}   // namespace dbus


#endif   // SIMPPL_TYPES_H
