#ifndef SIMPPL_DBUS_BUFFER_H
#define SIMPPL_DBUS_BUFFER_H


#include <stdlib.h>
#include <cstring>

#include "simppl/serialization.h"


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
   explicit
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
   

// FIXME make slim implementation in src file
template<size_t len>
struct Codec<FixedSizeBuffer<len>> : composite_signature<signature_chars<DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE>>
{
   static 
   void encode(Encoder& e, const FixedSizeBuffer<len>& b)
   {
      Encoder array = e.open_container(DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING);
      array.append_fixed_array(DBUS_TYPE_BYTE, b.buf, len);
   }
   
   
   static 
   void decode(Decoder& d, FixedSizeBuffer<len>& b)
   {
      Decoder array = d.recurse(DBUS_TYPE_ARRAY);
      
      unsigned char* buf; 
      int _len = len;
      dbus_message_iter_get_fixed_array(&array.native(), &buf, &_len);
      
      b.assign(buf);
      
      // advance to next element
      d.next();
   }
};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_BUFFER_H
