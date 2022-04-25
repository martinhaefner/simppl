#include "simppl/filedescriptor.h"

#include <cassert>


namespace {

    int noop(int)
    {
        // NOOP
        return 0;
    }
}


// ---------------------------------------------------------------------


namespace simppl
{

namespace dbus
{


FileDescriptor::FileDescriptor()
 : fd_(-1)
 , destructor_(&close)
{
   // NOOP
}


FileDescriptor::FileDescriptor(int fd)
 : fd_(fd)
 , destructor_(&close)
{
   assert(fd >= 0);
}


FileDescriptor::FileDescriptor(std::reference_wrapper<int> fd)
 : fd_(fd)
 , destructor_(&noop)
{
   assert(fd >= 0);
}


FileDescriptor::~FileDescriptor()
{
   if (fd_ != -1)
      (*destructor_)(fd_);
}


FileDescriptor::FileDescriptor(const FileDescriptor& rhs)
 : fd_(-1)
 , destructor_(rhs.destructor_)
{
   if (rhs.fd_ != -1)
   {
       if (destructor_ == &noop)
          fd_ = rhs.fd_;
       else
          fd_ = dup(rhs.fd_);
   }
}


FileDescriptor& FileDescriptor::operator=(const FileDescriptor& rhs)
{
   if (this != &rhs)
   {
      if (fd_ != -1)
         (*destructor_)(fd_);

      if (rhs.fd_ != -1)
      {
         if (rhs.destructor_ == &noop)
            fd_ = rhs.fd_;
         else
            fd_ = dup(rhs.fd_);
      }
      else
         fd_ = -1;

      destructor_ = rhs.destructor_;
   }

   return *this;
}


FileDescriptor::FileDescriptor(FileDescriptor&& rhs)
 : fd_(rhs.fd_)
 , destructor_(rhs.destructor_)
{
   rhs.fd_ = -1;
}


FileDescriptor& FileDescriptor::operator=(int fd)
{
   if (fd_ != -1)
      (*destructor_)(fd_);

   fd_ = fd;

   if (fd_ < 0)
      fd_ = -1;

   return *this;
}


FileDescriptor& FileDescriptor::operator=(FileDescriptor&& rhs)
{
   if (fd_ != -1)
      (*destructor_)(fd_);

   fd_ = rhs.fd_;
   rhs.fd_ = -1;

   destructor_ = rhs.destructor_;

   return *this;
}


int FileDescriptor::native_handle() const
{
   return fd_;
}


int FileDescriptor::release()
{
   int fd = fd_;
   fd_ = -1;

   return fd;
}


/*static*/
void FileDescriptorCodec::encode(DBusMessageIter& iter, const FileDescriptor& fd)
{
   int _fd = fd.native_handle();
   dbus_message_iter_append_basic(&iter, DBUS_TYPE_UNIX_FD, &_fd);
}


/*static*/
void FileDescriptorCodec::decode(DBusMessageIter& iter, FileDescriptor& fd)
{
   int _fd;
   simppl_dbus_message_iter_get_basic(&iter, &_fd, DBUS_TYPE_UNIX_FD);
   fd = _fd;
}


/*static*/
std::ostream& FileDescriptorCodec::make_type_signature(std::ostream& os)
{
   return os << DBUS_TYPE_UNIX_FD_AS_STRING;
}


}   // namespace dbus

}   // namespace simppl
