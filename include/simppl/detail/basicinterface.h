#ifndef SIMPPL_DETAIL_BASICINTERFACE_H
#define SIMPPL_DETAIL_BASICINTERFACE_H


// forward decl
struct DBusConnection;


namespace simppl
{

namespace dbus
{

namespace detail
{

struct BasicInterface
{
   BasicInterface(const BasicInterface&) = delete;
   BasicInterface& operator=(const BasicInterface&) = delete;
   
   BasicInterface();
    
   virtual ~BasicInterface();

   DBusConnection* conn_;
};

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_BASICINTERFACE_H
