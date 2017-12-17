#ifndef __SIMPPL_DBUS_BUFFER_H__
#define __SIMPPL_DBUS_BUFFER_H__


#include <stdlib.h>
#include <cstring>


namespace simppl
{

namespace dbus
{


template<size_t len>
struct FixedSizeBuffer
{
   FixedSizeBuffer(const FixedSizeBuffer& rhs)
   {
      if (rhs.self)
      {
         buf = new unsigned char[len];
         memcpy(buf, rhs.buf, len);
      }
      else
         buf = rhs.buf;
         
      self = rhs.self;
   }
   
   FixedSizeBuffer& operator=(const FixedSizeBuffer&) = delete;
   
   FixedSizeBuffer()
    : buf(nullptr)
    , self(false)
   {
      // NOOP
   }
   
   ~FixedSizeBuffer()
   {
      if (self)
         delete[] buf;
   }
   
   /**
    * No copy of data.
    */
   FixedSizeBuffer(void* buf)
    : buf((unsigned char*)buf)
    , self(false)
   {
      // NOOP
   }
   
   const void* ptr() const
   {
      return buf;
   }
   
   void* ptr()
   {
      return buf;
   }
   
   /**
    * Assigned buffer will copy the data to interal buffer which will 
    * be destroyed in destructor.
    */
   void assign(void* b)
   {
      if (!self)
         buf = new unsigned char[len];
         
      memcpy(buf, b, len);
      self = true;
   }
   
   unsigned char* buf;
   bool self;
};


}   // namespace dbus

}   // namespace simppl


#endif   // __SIMPPL_DBUS_BUFFER_H__
