#ifndef SIMPPL_CALLSTATE_H
#define SIMPPL_CALLSTATE_H


#include <cstdint>

#include "simppl/error.h"

#include "simppl/detail/error.h"


struct DBusMessage;


namespace simppl
{

namespace dbus
{

// forward decl
namespace detail {
    template<typename, typename, typename> struct CallbackHolder;
    template<typename, typename> struct PropertyCallbackHolder;
}


template<typename ErrorT>
struct TCallState
{
    template<typename, typename, typename> friend struct detail::CallbackHolder;
    template<typename, typename> friend struct detail::PropertyCallbackHolder;
    template<typename, int> friend struct ClientProperty;


   TCallState(TCallState&& st)
    : ex_(st.ex_.release())
    , serial_(st.serial_)
   {
      // NOOP
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
   const ErrorT& exception() const
   {
      return *ex_;
   }


private:

   explicit inline
   TCallState(uint32_t serial)
    : ex_()
    , serial_(serial)
   {
      // NOOP
   }


   /// @param ex The function takes ownership of the exception.
   explicit inline
   TCallState(ErrorT* ex)
    : ex_(ex)
    , serial_(SIMPPL_INVALID_SERIAL)
   {
      // NOOP
   }


   TCallState(DBusMessage& msg)
    : ex_()
    , serial_(SIMPPL_INVALID_SERIAL)
   {
      if (dbus_message_get_type(&msg) == DBUS_MESSAGE_TYPE_ERROR)
      {
         ex_.reset(new ErrorT);
         detail::ErrorFactory<ErrorT>::init(*ex_, msg);
      }
      else
         serial_ = dbus_message_get_reply_serial(&msg);
   }


   std::unique_ptr<ErrorT> ex_;
   uint32_t serial_;
};


typedef TCallState<Error> CallState;


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_CALLSTATE_H
