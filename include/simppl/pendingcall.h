#ifndef SIMPPL_PENDINGCALL_H
#define SIMPPL_PENDINGCALL_H


#include "simppl/types.h"

namespace simppl
{

namespace dbus
{


struct PendingCall
{
    PendingCall(uint32_t serial, DBusPendingCall* p);
    PendingCall();

    // only move
    PendingCall(PendingCall&& rhs);
    PendingCall& operator=(PendingCall&& rhs);

    // no copy
    PendingCall(const PendingCall&) = delete;
    PendingCall& operator=(const PendingCall&) = delete;

    /** request serial */
    uint32_t serial() const;

    DBusPendingCall* pending();

    /** cancel pending request */
    void cancel();

    void reset();

private:

    uint32_t serial_;
    pending_call_ptr_t pending_;
};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_PENDINGCALL_H
