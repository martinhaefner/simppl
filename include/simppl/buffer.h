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


namespace detail
{
   

template<size_t len>
struct Codec<FixedSizeBuffer<len>>
{
   static 
   void encode(Serializer& s, const FixedSizeBuffer<len>& b)
   {
      DBusMessageIter iter;

      dbus_message_iter_open_container(s.iter_, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &iter);
      dbus_message_iter_append_fixed_array(&iter, DBUS_TYPE_BYTE, &b.buf, len);      
      dbus_message_iter_close_container(s.iter_, &iter);
   }
   
   
   static 
   void decode(Deserializer& s, FixedSizeBuffer<len>& b)
   {
      DBusMessageIter iter;
      dbus_message_iter_recurse(s.iter_, &iter);
      
      unsigned char* buf; 
      int _len = len;
      dbus_message_iter_get_fixed_array(&iter, &buf, &_len);
      
      b.assign(buf);
      
      // advance to next element
      dbus_message_iter_next(s.iter_);
   }
};


template<size_t len>
struct make_type_signature<FixedSizeBuffer<len>>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return os << DBUS_TYPE_ARRAY_AS_STRING;
   }
};


}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // __SIMPPL_DBUS_BUFFER_H__
