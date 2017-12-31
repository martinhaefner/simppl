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
struct Codec<FixedSizeBuffer<len>>
{
   static 
   void encode(DBusMessageIter& iter, const FixedSizeBuffer<len>& b)
   {
      DBusMessageIter _iter;

      dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &_iter);
      dbus_message_iter_append_fixed_array(&_iter, DBUS_TYPE_BYTE, &b.buf, len);      
      dbus_message_iter_close_container(&iter, &_iter);
   }
   
   
   static 
   void decode(DBusMessageIter& iter, FixedSizeBuffer<len>& b)
   {
      DBusMessageIter _iter;
      simppl_dbus_message_iter_recurse(&iter, &_iter, DBUS_TYPE_ARRAY);
      
      unsigned char* buf; 
      int _len = len;
      dbus_message_iter_get_fixed_array(&_iter, &buf, &_len);
      
      b.assign(buf);
      
      // advance to next element
      dbus_message_iter_next(&iter);
   }
   
   
   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return os << DBUS_TYPE_ARRAY_AS_STRING;
   }
};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_BUFFER_H
