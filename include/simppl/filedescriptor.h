#ifndef __SIMPPL_DBUS_FILEDESCRIPTOR_H__
#define __SIMPPL_DBUS_FILEDESCRIPTOR_H__


namespace simppl
{

namespace dbus
{

class FileDescriptor
{
public:

   FileDescriptor();
   FileDescriptor(int fd);
   
   ~FileDescriptor();
   
   FileDescriptor(const FileDescriptor& rhs);
   FileDescriptor& operator=(const FileDescriptor& rhs);
   
   FileDescriptor(FileDescriptor&& rhs);
   FileDescriptor& operator=(FileDescriptor&& rhs);
   
   int native_handle() const;
   int release();
   
private:
   int fd_;
};

}

}


#endif   // __SIMPPL_DBUS_FILEDESCRIPTOR_H__
