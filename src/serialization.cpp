#include "simppl/detail/serialization.h"


Serializer::Serializer(size_t initial)
 : capacity_(initial)
 , buf_(capacity_ > 0 ? (char*)malloc(capacity_) : 0)
 , current_(buf_)
{
   // NOOP
}


void* Serializer::release()
{
   void* rc = buf_;
   buf_ = 0;
   return rc;
}


size_t Serializer::size() const
{
   if (capacity_ > 0)
   {
      assert(buf_);
      return current_ - buf_;
   }
   else
      return 0;
}


Serializer& Serializer::write(const std::string& str)
{
   int32_t len = str.size();
   enlarge(len + sizeof(len));
   
   memcpy(current_, &len, sizeof(len));
   current_ += sizeof(len);
   memcpy(current_, str.c_str(), len);
   current_ += len;
   
   return *this;
}


void Serializer::enlarge(size_t needed)
{
   size_t current = size();
   size_t estimated_capacity = current + needed;
   
   if (capacity_ < estimated_capacity)
   {
      while (capacity_ < estimated_capacity)
         capacity_ <<=1 ;
      
      buf_ = (char*)realloc(buf_, capacity_);
      current_ = buf_ + current;
   }
}


// ----------------------------------------------------------------------------


Deserializer::Deserializer(const void* buf, size_t capacity)
 : capacity_(capacity)
 , buf_((const char*)buf)
 , current_(buf_)
{
   // NOOP
}


Deserializer& Deserializer::read(char*& str)
{
   assert(str == 0);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free
   
   uint32_t len;
   read(len);
   
   if (len > 0)
   {
      str = allocate(len);
      memcpy(str, current_, len);
      current_ += len;
   }
   else
   {
      str = allocate(1);
      *str = '\0';
   }
   
   return *this;
}


Deserializer& Deserializer::read(std::string& str)
{
   uint32_t len;
   read(len);
   
   if (len > 0)
   {
      str.assign(current_, len);
      current_ += len;
   }
   else
      str.clear();

   return *this;
}