#include "simppl/stub.h"

#include "simppl/dispatcher.h"

#include "simppl/detail/util.h"
#include "simppl/clientside.h"

#include <cstring>
#include <chrono>
#include <cassert>


using namespace std::literals::chrono_literals;


namespace simppl
{

namespace dbus
{

StubBase::StubBase(const char* iface, const char* role)
 : iface_(detail::extract_interface(iface))
 , objectpath_(nullptr)
 , conn_state_(ConnectionState::Disconnected)
 , disp_(nullptr)
{
   assert(role);
   objectpath_ = detail::create_objectpath(iface_, role);

   busname_.reserve(strlen(this->iface()) + 1 + strlen(role));
   busname_ = this->iface();
   busname_ += ".";
   busname_ += role;
}


StubBase::StubBase(const char* iface, const char* busname, const char* objectpath)
 : iface_(detail::extract_interface(iface))
 , objectpath_(nullptr)
 , conn_state_(ConnectionState::Disconnected)
 , disp_(nullptr)
{
   assert(busname);
   assert(objectpath);

   objectpath_ = new char[strlen(objectpath)+1];
   strcpy(objectpath_, objectpath);

   busname_ = busname;
}


StubBase::~StubBase()
{
   if (disp_)
      disp_->remove_client(*this);

   delete[] iface_;
   delete[] objectpath_;
}


DBusConnection* StubBase::conn()
{
    return disp().conn_;
}


Dispatcher& StubBase::disp()
{
   assert(disp_);
   return *disp_;
}


DBusPendingCall* StubBase::send_request(const char* method_name, std::function<void(detail::Serializer&)> f, bool is_oneway)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), iface(), method_name));
    DBusPendingCall* pending = nullptr;

    detail::Serializer s(msg.get());
    f(s);

    if (!is_oneway)
    {
        int timeout = disp().request_timeout();

        if (detail::request_specific_timeout.count() > 0)
            timeout = detail::request_specific_timeout.count();

        dbus_connection_send_with_reply(disp().conn_, msg.get(), &pending, timeout);
    }
    else
    {
       // otherwise server would stop reading requests after a while
       dbus_message_set_no_reply(msg.get(), TRUE);

       dbus_connection_send(disp().conn_, msg.get(), nullptr);
       dbus_connection_flush(disp().conn_);
    }

    detail::request_specific_timeout = std::chrono::milliseconds(0);

    return pending;
}


void StubBase::connection_state_changed(ConnectionState state)
{
   if (conn_state_ != state)
   {
      conn_state_ = state;

      if (connected)
         connected(conn_state_);
   }
}


void StubBase::register_signal(ClientSignalBase& sigbase)
{
   assert(disp_);
   assert(signals_.find(sigbase.name()) == signals_.end());   // signal already attached by this stub

   disp_->register_signal(*this, sigbase);

   // TODO use intrusive container and iterate instead of using a map -> memory efficiency
   signals_[sigbase.name()] = &sigbase;
}


void StubBase::unregister_signal(ClientSignalBase& sigbase)
{
   assert(disp_);

   auto iter = signals_.find(sigbase.name());

   if (iter != signals_.end())
   {
      disp_->unregister_signal(*this, sigbase);
      signals_.erase(iter);
   }
}


void StubBase::attach_property(const char* name, std::function<void(detail::Deserializer&)> f)
{
   assert(disp_);

   if (properties_.empty())
      disp_->register_properties(*this);

   properties_[name] = f;
}


void StubBase::detach_property(const char* name)
{
   assert(disp_);

   properties_.erase(name);

   if (properties_.empty())
      disp_->unregister_properties(*this);
}


void StubBase::cleanup()
{
   // cleanup signal registrations
   for (auto& sig_entry : signals_)
   {
      disp_->unregister_signal(*this, *sig_entry.second);
   }

   signals_.clear();

   // cleanup property registration
   if (!properties_.empty())
      disp_->unregister_properties(*this);

   properties_.clear();

   disp_ = nullptr;
}


DBusPendingCall* StubBase::get_property_async(const char* name)
{
   message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Get"));
   DBusPendingCall* pending = nullptr;

   detail::Serializer s(msg.get());
   s << iface() << name;

   dbus_connection_send_with_reply(conn(), msg.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT);

   return pending;
}


message_ptr_t StubBase::get_property(const char* name)
{
   message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Get"));
   DBusPendingCall* pending = nullptr;

   detail::Serializer s(msg.get());
   s << iface() << name;

   dbus_connection_send_with_reply(conn(), msg.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT);

   dbus_pending_call_block(pending);

   msg.reset(dbus_pending_call_steal_reply(pending));
   dbus_pending_call_unref(pending);

   return msg;
}


void StubBase::set_property(const char* name, std::function<void(detail::Serializer&)> f)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Set"));

    detail::Serializer s(msg.get());
    s << iface() << name;
    f(s);   // and now serialize the variant

     DBusError err;
     dbus_error_init(&err);

     DBusMessage* reply = dbus_connection_send_with_reply_and_block(disp().conn_, msg.get(), DBUS_TIMEOUT_USE_DEFAULT, &err);

     // drop original message
     msg.reset(reply);

     // check for reponse
     if (dbus_error_is_set(&err))
     {
        // TODO make static function to throw from DBusError
        Error ex(err.name, err.message);

        dbus_error_free(&err);
        throw ex;
    }
}


DBusPendingCall* StubBase::set_property_async(const char* name, std::function<void(detail::Serializer&)> f)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Set"));

    detail::Serializer s(msg.get());
    s << iface() << name;
    f(s);   // and now serialize the variant

    DBusPendingCall* pending = nullptr;

    dbus_connection_send_with_reply(disp().conn_, msg.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT);

    return pending;
}


void StubBase::try_handle_signal(DBusMessage* msg)
{
   // FIXME better check -> include interface!
   if (!strcmp(dbus_message_get_member(msg), "PropertiesChanged"))
   {
      detail::Deserializer d(msg);

      std::string iface;
      d >> iface;
      // ignore interface name for now

      DBusMessageIter iter;
      dbus_message_iter_recurse(d.iter_, &iter);

      while(dbus_message_iter_get_arg_type(&iter) != 0)
      {
         DBusMessageIter item_iterator;
         dbus_message_iter_recurse(&iter, &item_iterator);

         detail::Deserializer s(&item_iterator);

         std::string property_name;
         s >> property_name;

         auto found = properties_.find(property_name);
         if (found != properties_.end())
            found->second(s);

         // advance to next element
         dbus_message_iter_next(&iter);
      }
   }
   else
   {
      auto iter = signals_.find(dbus_message_get_member(msg));
      if (iter != signals_.end())
      {
         iter->second->eval(msg);
      }
   }
}


}   // namespace dbus

}   // namespace simppl

