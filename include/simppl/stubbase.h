#ifndef SIMPPL_STUBBASE_H
#define SIMPPL_STUBBASE_H


#include <cassert>
#include <functional>
#include <map>

#include <dbus/dbus.h>

#include "simppl/callstate.h"
#include "simppl/detail/constants.h"
#include "simppl/detail/parented.h"

#include "simppl/connectionstate.h"


namespace simppl
{

namespace dbus
{

// forward decls
struct Dispatcher;
struct ClientSignalBase;

namespace detail
{
   struct Serializer;
};


struct StubBase
{
   template<typename... T> friend struct ClientSignal;
   template<typename... T> friend struct ClientRequest;
   template<typename, int> friend struct ClientProperty;
   template<typename, typename> friend struct ClientPropertyWritableMixin;

   friend struct Dispatcher;

   StubBase(const StubBase&) = delete;
   StubBase& operator=(const StubBase&) = delete;

protected:

   virtual ~StubBase();

public:

   std::function<void(ConnectionState)> connected;

   StubBase(const char* iface, const char* role);

   // FIXME use ObjectPath instead of const char*
   StubBase(const char* iface, const char* busname, const char* objectpath);

   inline
   const char* iface() const
   {
      return iface_;
   }

   inline
   const char* objectpath() const
   {
      return objectpath_;
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


protected:

   void cleanup();

   DBusPendingCall* send_request(const char* method_name, std::function<void(detail::Serializer&)> f, bool is_oneway);

   inline
   std::string busname() const
   {
      return busname_;
   }

   void register_signal(ClientSignalBase& sigbase);
   void unregister_signal(ClientSignalBase& sigbase);

   /**
    * Blocking call.
    */
   message_ptr_t get_property(const char* name);

   DBusPendingCall* get_property_async(const char* name);

   /**
    * Blocking call.
    */
   void set_property(const char* Name, std::function<void(detail::Serializer&)> f);
   
   DBusPendingCall* set_property_async(const char* Name, std::function<void(detail::Serializer&)> f);

   char* iface_;
   char* objectpath_;
   std::string busname_;
   ConnectionState conn_state_;

   Dispatcher* disp_;
   std::map<std::string, ClientSignalBase*> signals_;
};

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_STUBBASE_H
