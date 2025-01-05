#ifndef SIMPPL_CALLSTATE_H
#define SIMPPL_CALLSTATE_H


#include <cstdint>
#include <cstring>

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


struct CallState
{
   template<typename, typename, typename> friend struct detail::CallbackHolder;
   template<typename, typename> friend struct detail::PropertyCallbackHolder;
   template<typename, int> friend struct ClientProperty;
   friend struct StubBase;    
    

   CallState(CallState&& st)
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
   const char* what() const
   {
      return ex_->what();
   }

   inline
   const Error& exception() const
   {
      return *ex_;
   }
   
   template<typename ErrorT>
   inline
   const ErrorT* as() const
   {
	   const ErrorT* rc = nullptr;
	   
	   if (ex_)	   
		   rc = dynamic_cast<const ErrorT*>(ex_.get());		   	   
	   
	   return rc;
   }


private:

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


   CallState(DBusMessage& msg);   
   
   template<typename ErrorT>
   CallState(DBusMessage& msg, ErrorT* /*type identity only*/)
    : ex_()
    , serial_(SIMPPL_INVALID_SERIAL)
   {
      if (dbus_message_get_type(&msg) == DBUS_MESSAGE_TYPE_ERROR)
      {
		 if (!strcmp(dbus_message_get_signature(&msg), DBUS_TYPE_STRING_AS_STRING))
  		 {	
			ex_.reset(new Error);			
			detail::ErrorFactory<Error>::init(*ex_, msg);			
		 }
		 else			
		 {    
			ErrorT* err = new ErrorT;			 
            detail::ErrorFactory<ErrorT>::init(*err, msg);
            ex_.reset(err);         
		 }		 
      }
      else
         serial_ = dbus_message_get_reply_serial(&msg); 
   }


   std::unique_ptr<Error> ex_;
   uint32_t serial_;
};



}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_CALLSTATE_H
