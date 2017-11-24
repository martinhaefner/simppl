#ifndef SIMPPL_TYPES_H
#define SIMPPL_TYPES_H


#include <memory>

#include <dbus/dbus.h>


namespace simppl
{

namespace dbus
{


typedef std::unique_ptr<::DBusMessage, void(*)(::DBusMessage*)> message_ptr_t;

inline
message_ptr_t make_message(DBusMessage* p)
{
    return message_ptr_t(p, &dbus_message_unref);
}


typedef std::shared_ptr<DBusPendingCall> pending_call_ptr_t;

inline
pending_call_ptr_t make_pending_call(DBusPendingCall* p)
{
    return pending_call_ptr_t(p, &dbus_pending_call_unref);
}


}   // namespace simppl

}   // namespace dbus


#endif   // SIMPPL_TYPES_H
