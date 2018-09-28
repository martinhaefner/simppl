#ifndef SIMPPL_STUBBASE_H
#define SIMPPL_STUBBASE_H


#include <cassert>
#include <functional>
#include <map>
#include <string>

#include <dbus/dbus.h>

#include "simppl/callstate.h"
#include "simppl/pendingcall.h"
#include "simppl/detail/constants.h"

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
   struct Deserializer;
}


struct StubBase
{
   template<typename... T> friend struct ClientSignal;
   template<typename... T> friend struct ClientMethod;
   template<typename, int> friend struct ClientProperty;
   template<typename, typename> friend struct ClientPropertyWritableMixin;

   friend struct Dispatcher;

   StubBase(const StubBase&) = delete;
   StubBase& operator=(const StubBase&) = delete;

protected:

   virtual ~StubBase();

   void init(char* iface, const char* role);
   void init(char* iface, const char* busname, const char* objectpath);


public:

   std::function<void(ConnectionState)> connected;

   StubBase();

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

   inline
   std::string busname() const
   {
      return busname_;
   }


protected:

   void cleanup();

   PendingCall send_request(const char* method_name, std::function<void(detail::Serializer&)>&& f, bool is_oneway);

   message_ptr_t send_request_and_block(const char* method_name, std::function<void(detail::Serializer&)>&& f, bool is_oneway);

   void register_signal(ClientSignalBase& sigbase);
   void unregister_signal(ClientSignalBase& sigbase);

   void attach_property(const char* name, std::function<void(detail::Deserializer&)>&& f);
   void detach_property(const char* name);

   /**
    * Blocking call.
    */
   message_ptr_t get_property(const char* name);

   PendingCall get_property_async(const char* name);

   /**
    * Blocking call.
    */
   void set_property(const char* Name, std::function<void(detail::Serializer&)>&& f);

   PendingCall set_property_async(const char* Name, std::function<void(detail::Serializer&)>&& f);

   char* iface_;
   char* objectpath_;
   std::string busname_;
   ConnectionState conn_state_;

   Dispatcher* disp_;

   // FIXME use linked lists instead of map...
   std::map<std::string, ClientSignalBase*> signals_;
   std::map<std::string, std::function<void(detail::Deserializer&)>> properties_;
};

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_STUBBASE_H
