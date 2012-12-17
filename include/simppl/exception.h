#ifndef SIMPPL_EXCEPTION_H
#define SIMPPL_EXCEPTION_H


#include <exception>
#include <cstdint>

#include "simppl/detail/constants.h"


/// Base class for all IPC related exceptions.
struct IPCException : std::exception
{
   inline
   IPCException()
    : sequence_nr_(INVALID_SEQUENCE_NR)
   {
      // NOOP
   }

   explicit inline
   IPCException(uint32_t sequence_nr)
    : sequence_nr_(sequence_nr)
   {
      // NOOP
   }
   
   inline
   uint32_t sequenceNr() const
   {
      return sequence_nr_;
   }
   
private:
   
   uint32_t sequence_nr_;
};


// FIXME add a namespace simppl to everything in here
// FIXME move private stuff to detail namespace


/// blocking calls will throw, eventloop driven approach will call separate handlers
struct RuntimeError : IPCException
{
   explicit
   RuntimeError(int error);
   
   RuntimeError(int error, const char* message);
   
   ~RuntimeError() throw();
 
   /// this shoudl be a move and will probably be optimized out
   RuntimeError(const RuntimeError&);
   
   RuntimeError& operator=(const RuntimeError&) = delete;
   
   inline
   const char* what() const throw()
   {
      return message_;
   }
   
   inline
   int error() const
   {
      return error_;
   }
   
private:
   
   friend struct Dispatcher;
   
   /// only for dispatcher
   RuntimeError(int error, const char* message, uint32_t sequence_nr);
   
   int error_;
   char* message_;
   char nullmsg_[1];
};


struct TransportError : IPCException
{
   explicit
   TransportError(int errno__, uint32_t sequence_nr);
   
   inline
   int getErrno() const
   {
      return errno_;
   }
   
   const char* what() const throw();
   
   int errno_;
   mutable char buf_[128];
};


#endif   // SIMPPL_EXCEPTION_H
