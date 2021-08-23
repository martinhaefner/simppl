#ifndef SIMPPL_DBUS_FILEDESCRIPTOR_H
#define SIMPPL_DBUS_FILEDESCRIPTOR_H


#include "simppl/serialization.h"

#include <unistd.h>
#include <functional>


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

   /**
    * RAII behaviour: destroys the fd by calling close on object destruction.
    */
   explicit
   FileDescriptor(int fd);

   /**
    * Non-RAII behaviour: keeps fd open on object destruction.
    */
   explicit
   FileDescriptor(std::reference_wrapper<int> fd);

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
   int (*destructor_)(int);
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
