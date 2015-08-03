#ifndef SIMPPL_CALLSTATE_H
#define SIMPPL_CALLSTATE_H


#include <memory>

#include "simppl/error.h"


namespace simppl
{
   
namespace ipc
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
   
   /// FIXME not inline
   inline
   void throw_exception() const
   {
      ex_->_throw();
   }
   
   
private:
   
   std::unique_ptr<Error> ex_;
   uint32_t sequence_nr_;
};

}   // namespace ipc
   
}   // namespace simppl


#endif   // SIMPPL_CALLSTATE_H
