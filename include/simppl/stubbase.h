#ifndef SIMPPL_STUBBASE_H
#define SIMPPL_STUBBASE_H


#include <cassert>
#include <functional>
#include <map>
#include <sstream>
#include <iostream>  // FIXME remove this

#include <dbus/dbus.h>

#include "simppl/detail/constants.h"
#include "simppl/detail/parented.h"

#include "simppl/connectionstate.h"


namespace simppl
{

namespace ipc
{

// forward decls
struct Dispatcher;
struct ClientResponseBase;
struct ClientSignalBase;

namespace detail
{
   struct Serializer;
};


struct StubBase
{
   template<typename... T> friend struct ClientSignal;
   template<typename... T> friend struct ClientRequest;
   template<typename, typename> friend struct ClientAttribute;
   friend struct Dispatcher;

protected:

   virtual ~StubBase();

public:

   std::function<void(ConnectionState)> connected;

   StubBase(const char* iface, const char* role);

   inline
   const char* iface() const
   {
      return iface_;
   }

   inline
   const char* role() const
   {
      return role_;
   }

   // FIXME make this function some generic helper.
   // FIXME either const char* or string return, must be clean all-over
   std::string objectpath() const
   {
       std::ostringstream opath;
       opath << "/" << iface() << "." << role();

       std::string objectpath = opath.str();

       std::for_each(objectpath.begin(), objectpath.end(), [](char& c){
       if (c == '.')
           c = '/';
       });

       return objectpath;
   }

   Dispatcher& disp();

   /// FIXME protected?!
   void try_handle_signal(DBusMessage* msg);

   void connection_state_changed(ConnectionState state);

   DBusConnection* conn();

   inline
   bool is_connected() const
   {
       return conn_state_ == ConnectionState::Connected;
   }

   // FIXME
   inline
   bool connect()
   {
       return true;
   }

protected:

   std::string boundname() const;

   void sendSignalRegistration(ClientSignalBase& sigbase);
   void sendSignalUnregistration(ClientSignalBase& sigbase);

   void getProperty(const char* name, void(*callback)(DBusPendingCall*, void*), void* user_data);

   char iface_[128];
   const char* role_;
   ConnectionState conn_state_;

   Dispatcher* disp_;
   std::map<std::string, ClientSignalBase*> signals_;
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_STUBBASE_H
