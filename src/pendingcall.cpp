#include "simppl/pendingcall.h"

#include "simppl/detail/constants.h"


namespace simppl
{

namespace dbus
{


PendingCall::PendingCall(uint32_t serial, DBusPendingCall* p)
 : serial_(serial)
{
	if (p)
        pending_ = make_pending_call(p);
}


PendingCall::PendingCall()
 : serial_(SIMPPL_INVALID_SERIAL)
{
    // NOOP
}


PendingCall::PendingCall(PendingCall&& rhs)
{
	serial_ = rhs.serial_;
    pending_ = rhs.pending_;

    rhs.reset();
}


PendingCall& PendingCall::operator=(PendingCall&& rhs)
{
	serial_ = rhs.serial_;
    pending_ = rhs.pending_;

    rhs.reset();
    return *this;
}


PendingCall::PendingCall(const PendingCall& rhs)
 : serial_(rhs.serial_)
 , pending_(rhs.pending_)
{
	// NOOP
}


PendingCall& PendingCall::operator=(const PendingCall& rhs)
{
	if (&rhs != this)
	{
		serial_ = rhs.serial_;
		pending_ = rhs.pending_;
	}
	
	return *this; 
}

uint32_t PendingCall::serial() const
{
    return serial_;
}


DBusPendingCall* PendingCall::pending()
{
    DBusPendingCall* rc = pending_.get();
    return rc;
}


void PendingCall::cancel()
{
    if (pending_)
		dbus_pending_call_cancel(pending_.get());
	
	reset();
}


void PendingCall::reset()
{
    serial_ = SIMPPL_INVALID_SERIAL;
    pending_.reset();
}


}   // namespace dbus

}   // namespace simppl
