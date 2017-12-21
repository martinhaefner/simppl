#ifndef __SIMPPL_DBUS_FILEDESCRIPTOR_H__
#define __SIMPPL_DBUS_FILEDESCRIPTOR_H__


#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{


/**
 * RAII object.
 */
class FileDescriptor
{
public:

   FileDescriptor();
   
   explicit
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


struct FileDescriptorCodec
{
   static 
   void encode(DBusMessageIter& iter, const FileDescriptor& fd);
   
   static 
   void decode(DBusMessageIter& iter, FileDescriptor& fd);
   
   static 
   std::ostream& make_type_signature(std::ostream& os);
};


template<>
struct Codec<FileDescriptor> : public FileDescriptorCodec {};


}   // namespace dbus

}   // namespace simppl


#endif   // __SIMPPL_DBUS_FILEDESCRIPTOR_H__
