#ifndef SIMPPL_ERROR_H
#define SIMPPL_ERROR_H


#include <exception>
#include <cstdint>
#include <string>

#include "simppl/detail/constants.h"


struct DBusMessage;


namespace simppl
{

namespace dbus
{

/// Base class for all IPC related exceptions.
struct Error : std::exception
{
   inline
   Error()
    : sequence_nr_(INVALID_SEQUENCE_NR)
   {
      // NOOP
   }

   explicit inline
   Error(uint32_t sequence_nr)
    : sequence_nr_(sequence_nr)
   {
      // NOOP
   }

   inline
   uint32_t sequenceNr() const
   {
      return sequence_nr_;
   }

   virtual void _throw() = 0;


private:

   uint32_t sequence_nr_;
};


/// blocking calls will throw, eventloop driven approach will call separate handlers
struct RuntimeError : Error
{
   explicit
   RuntimeError(int error);

   RuntimeError(int error, const char* message);

   /// only for dispatcher
   RuntimeError(int error, const char* message, uint32_t sequence_nr);

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
   int error() const throw()
   {
      return error_;
   }

   /**
    * FIXME internal -> friend
    */
   void _throw()
   {
      throw *this;
   }

   /**
    * FIXME internal -> friend
    */
   virtual DBusMessage* create_dbus_message(DBusMessage& request) const;

private:

   int error_;
   char* message_;
   char nullmsg_[1];
};


struct UserError : public RuntimeError
{
    UserError(const char* errname, const char* errmsg);

    DBusMessage* create_dbus_message(DBusMessage& request) const override;

    const char* what() const throw();

private:

    std::string errname_;
    std::string errmsg_;

    mutable std::string what_;
};


struct TransportError : Error
{
   explicit
   TransportError(int errno__, uint32_t sequence_nr);

   inline
   int getErrno() const throw()
   {
      return errno_;
   }

   void _throw()
   {
      throw *this;
   }

   const char* what() const throw();

private:

   int errno_;
   mutable char buf_[128];
};

}   // namespace simppl

}   // namespace dbus


#endif   // SIMPPL_ERROR_H
