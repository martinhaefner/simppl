#ifndef SIMPPL_CALLSTATE_H
#define SIMPPL_CALLSTATE_H


#include <cstdint>

#include "simppl/error.h"


struct DBusMessage;


namespace simppl
{

namespace dbus
{

struct CallState
{
   explicit inline
   CallState(uint32_t serial)
    : ex_()
    , serial_(serial)
   {
      // NOOP
   }

   /// @param ex The function takes ownership of the exception.
   explicit inline
   CallState(Error* ex)
    : ex_(ex)
    , serial_(SIMPPL_INVALID_SERIAL)
   {
      // NOOP
   }

   CallState(CallState&& st)
    : ex_(st.ex_.release())
    , serial_(st.serial_)
   {
      // NOOP
   }

   CallState(DBusMessage&);

   // FIXME why is this necessary, we are not copyable by design?!
   CallState(const CallState& st)
    : ex_()
    , serial_(st.serial_)
   {
      ex_.reset(const_cast<CallState&>(st).ex_.release());
   }

   explicit inline
   operator bool() const
   {
      return ex_.get() ? false : true;
   }


   inline
   uint32_t serial() const
   {
      return ex_.get() ? ex_->serial() : serial_;
   }

   inline
   const char* const what() const
   {
      return ex_->what();
   }

   inline
   const Error& exception() const
   {
      return *ex_;
   }

   void throw_exception() const;


private:

   std::unique_ptr<Error> ex_;
   uint32_t serial_;
};

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_CALLSTATE_H
