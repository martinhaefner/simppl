#ifndef SIMPPL_DBUS_FILEDESCRIPTOR_H
#define SIMPPL_DBUS_FILEDESCRIPTOR_H


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
   FileDescriptor& operator=(int fd);

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


/**
 * File descriptors must be valid for transfer, otherwise the fd will
 * not be part of the message!
 */
template<>
struct Codec<FileDescriptor> : public FileDescriptorCodec {};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_FILEDESCRIPTOR_H
