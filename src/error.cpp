#include "simppl/error.h"
#include <iostream>   // FIXME


#include <dbus/dbus.h>

#include <cassert>
#include <cstring>


namespace simppl
{

namespace dbus
{

RuntimeError::RuntimeError(int error)
 : RuntimeError(error, nullptr)
{
   // NOOP
}


RuntimeError::RuntimeError(int error, const char* message)
 : Error()
 , error_(error)
 , message_(message ? new char[::strlen(message)+1] : nullmsg_)
{
   //assert(error);
   nullmsg_[0] = '\0';

   if (message)
      ::strcpy(message_, message);
}


RuntimeError::~RuntimeError() throw()
{
   if (message_ != nullmsg_)
      delete[] message_;
}


RuntimeError::RuntimeError(const RuntimeError& rhs)
 : Error()
 , error_(rhs.error_)
 , message_(nullmsg_)
{
   if (rhs.message_ != rhs.nullmsg_)
   {
      nullmsg_[0] = '\0';
      message_ = new char[strlen(rhs.message_)+1];
      strcpy(message_, rhs.message_);
   }
}


RuntimeError::RuntimeError(int error, const char* message, uint32_t sequence_nr)
 : Error(sequence_nr)
 , error_(error)
 , message_(message ? new char[::strlen(message)+1] : nullmsg_)
{
   assert(error);
   nullmsg_[0] = '\0';

   if (message)
      ::strcpy(message_, message);
}


DBusMessage* RuntimeError::create_dbus_message(DBusMessage& request) const
{
    return dbus_message_new_error_printf(&request, DBUS_ERROR_FAILED, "%d %s", error(), what()?what():"");
}


// ---------------------------------------------------------------------------


UserError::UserError(const char* errname, const char* errmsg)
 : RuntimeError(0, 0)
 , errname_(errname)
 , errmsg_()
{
    if (errmsg)
        errmsg_ = errmsg;
}


DBusMessage* UserError::create_dbus_message(DBusMessage& request) const
{
    return dbus_message_new_error(&request, errname_.c_str(), errmsg_.c_str());
}


const char* UserError::what() const throw()
{
    if (what_.empty())
    {
        what_ = errname_;

        if (!errmsg_.empty())
        {
            what_ += ": ";
            what_ += errmsg_;
        }
    }

    return what_.c_str();
}


// ---------------------------------------------------------------------------


TransportError::TransportError(int errno__, uint32_t sequence_nr)
 : Error(sequence_nr)
 , errno_(errno__)
{
   buf_[0] = '\0';
}



const char* TransportError::what() const throw()
{
   if (!buf_[0])
   {
      char buf[128];
      strcpy(buf_, strerror_r(errno_, buf, sizeof(buf)));
   }

   return buf_;
}

}   // namespace dbus

}   // namespace simppl
