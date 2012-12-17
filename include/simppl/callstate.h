#ifndef SIMPPL_CALLSTATE_H
#define SIMPPL_CALLSTATE_H


#include <memory>

#include "simppl/exception.h"


struct CallState
{
   explicit inline
   CallState(uint32_t sequenceNr)
    : ex_()
    , sequence_nr_(sequenceNr)
   {
      // NOOP
   }
   
   explicit inline
   CallState(IPCException* ex)
    : ex_(ex)
    , sequence_nr_(INVALID_SEQUENCE_NR)
   {
      // NOOP
   }
   
   inline
   operator const void*() const
   {
      return ex_.get() ? 0 : this;
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
   const IPCException& exception() const
   {
      return *ex_;
   }
   
   
private:
   
   std::unique_ptr<IPCException> ex_;
   uint32_t sequence_nr_;
};


#endif   // SIMPPL_CALLSTATE_H