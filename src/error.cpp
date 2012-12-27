#include "simppl/error.h"


#include <cassert>
#include <cstring>


namespace simppl
{
   
namespace ipc
{

RuntimeError::RuntimeError(int error)
 : Error()
 , error_(error)
 , message_(nullmsg_)
{
   assert(error);   // zero is reserved value for "no error"
}


RuntimeError::RuntimeError(int error, const char* message)
 : Error()
 , error_(error)
 , message_(message ? new char[::strlen(message)+1] : nullmsg_)
{
   assert(error);
   
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
 , message_(rhs.message_)
{
   RuntimeError& that = const_cast<RuntimeError&>(rhs);
   that.error_ = 0;
   that.message_ = nullptr;
}


RuntimeError::RuntimeError(int error, const char* message, uint32_t sequence_nr)
 : Error(sequence_nr)
 , error_(error)
 , message_(message ? new char[::strlen(message)+1] : nullmsg_)
{
   assert(error);
   
   if (message)
      ::strcpy(message_, message);
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

}   // namespace ipc

}   // namespace simppl
