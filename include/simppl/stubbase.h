#ifndef SIMPPL_STUBBASE_H
#define SIMPPL_STUBBASE_H


#include <functional>
#include <string>
#include <vector>

#include <dbus/dbus.h>

#include "simppl/callstate.h"
#include "simppl/pendingcall.h"
#include "simppl/variant.h"

#include "simppl/detail/constants.h"
#include "simppl/detail/holders.h"

#include "simppl/connectionstate.h"


namespace simppl
{

namespace dbus
{

// forward decls
struct Dispatcher;
struct ClientSignalBase;
struct ClientPropertyBase;


struct StubBase
{
   typedef detail::InterimGetAllPropertiesCallbackHolder getall_properties_holder_type;

   template<typename... T> friend struct ClientSignal;
   template<typename... T> friend struct ClientMethod;
   template<typename, int> friend struct ClientProperty;
   template<typename, typename> friend struct ClientPropertyWritableMixin;

   friend struct Dispatcher;
   friend struct ClientPropertyBase;
   friend struct detail::GetAllPropertiesHolder;

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
   const char* iface(std::size_t index = 0) const
   {
      return ifaces_[index].c_str();
   }

   inline
   const char* objectpath() const
   {
      return objectpath_;
   }

   Dispatcher& disp();

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

   /**
    * Implementation of org.freedesktop.DBus.Properties.GetAll(). Blocking call.
    *
    * Before calling this method all Properties callbacks shall be installed.
    * The properties callbacks will then be called before this function returns.
    *
    * Issues a call like this:
    * dbus-send --print-reply --dest=test.Properties.s /test/Properties/s org.freedesktop.DBus.Properties.GetAll string:test.Properties
    *
    * @TODO complete async support
    */
   void get_all_properties();

   /**
    * Implementation of org.freedesktop.DBus.Properties.GetAll(). Asynchronous call.
    *
    * Before calling this method all Properties callbacks shall be installed. You may install
    * only some of the property callbacks. If a callback is not registered the property will
    * just omitted.
    *
    * The asynchronous return will arrive as soon as call the property callbacks are evaluated.
    */
   getall_properties_holder_type get_all_properties_async();


protected:

   void try_handle_signal(DBusMessage* msg);

   void connection_state_changed(ConnectionState state);

   void cleanup();

   PendingCall send_request(const char* method_name, std::function<void(DBusMessageIter&)>&& f, bool is_oneway);

   message_ptr_t send_request_and_block(const char* method_name, std::function<void(DBusMessageIter&)>&& f, bool is_oneway);

   void register_signal(ClientSignalBase& sigbase);
   void unregister_signal(ClientSignalBase& sigbase);

   void attach_property(ClientPropertyBase* prop);
   void detach_property(ClientPropertyBase* prop);

   /**
    * Blocking call.
    */
   message_ptr_t get_property(const char* name);

   PendingCall get_property_async(const char* name);

   /**
    * Blocking call.
    */
   void set_property(const char* Name, std::function<void(DBusMessageIter&)>&& f);

   /**
    * Just register the property within the stub.
    */
   void add_property(ClientPropertyBase* property);

   PendingCall set_property_async(const char* Name, std::function<void(DBusMessageIter&)>&& f);

   /**
    * Second part of get_all_properties_async. Once the callback arrives
    * the property callbacks have to be called.
    */
   simppl::dbus::CallState get_all_properties_handle_response(DBusMessage& response);

   std::vector<std::string> ifaces_;
   char* objectpath_;
   std::string busname_;
   ConnectionState conn_state_;

   Dispatcher* disp_;

   ClientSignalBase* signals_;       ///< attached signals

   std::vector<std::pair<ClientPropertyBase*, bool /*attached*/>> properties_;   ///< all properties TODO maybe take another container...
   int attached_properties_;         ///< attach counter
};

}   // namespace dbus

}   // namespace simppl


inline
void operator>>(simppl::dbus::detail::InterimGetAllPropertiesCallbackHolder&& r, const std::function<void(simppl::dbus::CallState)>& f)
{
   dbus_pending_call_set_notify(r.pc_.pending(),
                                &simppl::dbus::detail::GetAllPropertiesHolder::pending_notify,
                                new simppl::dbus::detail::GetAllPropertiesHolder(f, r.stub_),
                                &simppl::dbus::detail::GetAllPropertiesHolder::_delete);
}


#endif   // SIMPPL_STUBBASE_H
