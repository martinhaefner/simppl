#ifndef SIMPPL_CALLSTATE_H
#define SIMPPL_CALLSTATE_H


#include <memory>
#include <cassert>

#include "simppl/error.h"


struct DBusMessage;


namespace simppl
{
   
namespace dbus
{

struct CallState
{
   explicit inline
   CallState(uint32_t sequenceNr)
    : ex_()
    , sequence_nr_(sequenceNr)
   {
      // NOOP
   }
   
   /// @param ex The function takes ownership of the exception.
   explicit inline
   CallState(Error* ex)
    : ex_(ex)
    , sequence_nr_(INVALID_SEQUENCE_NR)
   {
      // NOOP
   }
   
   CallState(CallState&& st)
    : ex_(st.ex_.release())
    , sequence_nr_(st.sequence_nr_)
   {
      // NOOP
   }
   
   CallState(DBusMessage&);

   // FIXME why is this necessary, we are not copyable by design?!
   CallState(const CallState& st)
    : ex_()
    , sequence_nr_(st.sequence_nr_)
   {
      ex_.reset(const_cast<CallState&>(st).ex_.release());
   }
   
   explicit inline
   operator bool() const
   {
      return ex_.get() ? false : true;
   } 
   
   
   inline
   uint32_t sequenceNr() const
   {
      return ex_.get() ? ex_->sequenceNr() : sequence_nr_;
   }
   
   inline
   bool isTransportError() const
   {
      return dynamic_cast<const TransportError*>(ex_.get());
   }
   
   inline
   bool isRuntimeError() const
   {
      return dynamic_cast<const RuntimeError*>(ex_.get());
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
   uint32_t sequence_nr_;
};

}   // namespace dbus
   
}   // namespace simppl


#endif   // SIMPPL_CALLSTATE_H
