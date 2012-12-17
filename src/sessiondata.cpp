#include "simppl/sessiondata.h"


SessionData::SessionData()
 : fd_(-1)
 , data_(nullptr)
 , destructor_(nullptr)
{
   // NOOP
}


SessionData::SessionData(SessionData&& rhs)
 : fd_(rhs.fd_)
 , data_(rhs.data_)
 , destructor_(rhs.destructor_)
{
   rhs.fd_ = -1;
   rhs.data_ = nullptr;
   rhs.destructor_ = nullptr;
}

   
SessionData::SessionData(int fd, void* data, void(*destructor)(void*))
 : fd_(fd)
 , data_(data)
 , destructor_(destructor)
{
   // NOOP
}

   
SessionData& SessionData::operator=(SessionData&& rhs)
{
   if (this != &rhs)
   {
      fd_ = rhs.fd_;
      data_ = rhs.data_;
      destructor_ = rhs.destructor_;
      
      rhs.fd_ = -1;
      rhs.data_ = nullptr;
      rhs.destructor_ = nullptr;
   }
   
   return *this;
}

   
SessionData::~SessionData()
{
   if (data_ && destructor_)
      (*destructor_)(data_);
}
