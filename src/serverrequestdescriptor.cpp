#include "simppl/serverrequestdescriptor.h"

#include "simppl/detail/constants.h"

#include <dbus/dbus.h>


namespace simppl
{
   
namespace dbus
{


ServerRequestDescriptor::ServerRequestDescriptor()
 : requestor_(nullptr)
 , msg_(nullptr)
{
   // NOOP
}


ServerRequestDescriptor::ServerRequestDescriptor(ServerRequestDescriptor&& rhs)
 : requestor_(rhs.requestor_)
 , msg_(rhs.msg_)
{
   rhs.requestor_ = nullptr;
   rhs.msg_ = nullptr;
}


ServerRequestDescriptor::~ServerRequestDescriptor()
{
   clear();
}


ServerRequestDescriptor& ServerRequestDescriptor::operator=(ServerRequestDescriptor&& rhs)
{
   if (this != &rhs)
   {
      msg_ = rhs.msg_;
      requestor_ = rhs.requestor_;
      
      rhs.msg_ = nullptr;
      rhs.requestor_ = nullptr;
   }
      
   return *this;
}


ServerRequestDescriptor& ServerRequestDescriptor::set(ServerMethodBase* requestor, DBusMessage* msg)
{
   clear();
   
   requestor_ = requestor;
   msg_ = msg;
   
   if (msg_)
      dbus_message_ref(msg_);
   
   return *this;
}


void ServerRequestDescriptor::clear()
{
   if (msg_)
   {
      dbus_message_unref(msg_);
      msg_ = nullptr;
   }
   
   requestor_ = nullptr;
}


ServerRequestDescriptor::operator const void*() const
{
   return requestor_;
}


}   // namespace dbus

}   // namespace simppl

