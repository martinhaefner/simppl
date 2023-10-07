#include "simppl/stub.h"
#include <iostream>

#include "simppl/dispatcher.h"
#include "simppl/string.h"

#include "simppl/detail/util.h"
#include "simppl/clientside.h"

#include <cstring>
#include <chrono>
#include <cassert>


namespace {

struct TimeoutRAIIHelper
{
    TimeoutRAIIHelper(simppl::dbus::Dispatcher& disp)
     : timeout_(disp.request_timeout())
    {
        if (simppl::dbus::detail::request_specific_timeout.count() > 0)
            timeout_ = simppl::dbus::detail::request_specific_timeout.count();
    }

    ~TimeoutRAIIHelper()
    {
        simppl::dbus::detail::request_specific_timeout = std::chrono::milliseconds(0);
    }

    operator int()
    {
        return timeout_;
    }

    int timeout_;
};

}   // namespace


// ---------------------------------------------------------------------


using namespace std::literals::chrono_literals;


namespace simppl
{

namespace dbus
{

StubBase::StubBase()
 : get_all_properties(*this)
 , objectpath_(nullptr)
 , conn_state_(ConnectionState::Disconnected)
 , disp_(nullptr)
 , signals_(nullptr)
 , attached_properties_(0)
{
    // NOOP
}


StubBase::~StubBase()
{
   if (disp_)
      disp_->remove_client(*this);

   delete[] objectpath_;
}


void StubBase::init(char* iface, const char* busname, const char* objectpath)
{
    assert(busname);
    assert(objectpath);

    ifaces_ = detail::extract_interfaces(1, iface);

    objectpath_ = new char[strlen(objectpath)+1];
    strcpy(objectpath_, objectpath);

    busname_ = busname;

    free(iface);
}


void StubBase::init(char* iface, const char* role)
{
    assert(role);

    ifaces_ = detail::extract_interfaces(1, iface);

    objectpath_ = detail::create_objectpath(this->iface(), role);

    busname_.reserve(strlen(this->iface()) + 1 + strlen(role));
    busname_ = this->iface();
    busname_ += ".";
    busname_ += role;

    free(iface);
}


void StubBase::add_property(ClientPropertyBase* property)
{
    properties_.emplace_back(property, false);
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


void StubBase::get_all_properties_request()
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "GetAll"));
    DBusPendingCall* pending = nullptr;

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);

    encode(iter, iface());

    dbus_connection_send_with_reply(conn(), msg.get(), &pending, TimeoutRAIIHelper(disp()));

    dbus_pending_call_block(pending);

    msg.reset(dbus_pending_call_steal_reply(pending));
    dbus_pending_call_unref(pending);

    auto cs = get_all_properties_handle_response(*msg, true);

    // unused
    (void)cs;
}


simppl::dbus::CallState StubBase::get_all_properties_handle_response(DBusMessage& response, bool __throw)
{
    DBusMessageIter iter;
    dbus_message_iter_init(&response, &iter);

    // blocking version -> throw exception if error occurred
    if (__throw)
    {
        if (dbus_message_get_type(&response) == DBUS_MESSAGE_TYPE_ERROR)
        {
            simppl::dbus::Error err;
            detail::ErrorFactory<simppl::dbus::Error>::init(err, response);

            throw err;
        }
    }

    CallState cs(response);

    if (cs)
    {
        // open array container
        DBusMessageIter _iter;
        dbus_message_iter_recurse(&iter, &_iter);

        while(dbus_message_iter_get_arg_type(&_iter) != 0)
        {
            // open dict container
            DBusMessageIter __iter;
            dbus_message_iter_recurse(&_iter, &__iter);

            std::string propname;
            decode(__iter, propname);

            // get property by name
            auto propiter = std::find_if(properties_.begin(), properties_.end(), [propname](auto& pair){ return propname == pair.first->name_; });

            // get value and call
            if (propiter != properties_.end())
                propiter->first->eval(&__iter);

            dbus_message_iter_next(&_iter);
        }
    }

    return cs;
}


StubBase::getall_properties_holder_type StubBase::get_all_properties_request_async()
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "GetAll"));
    DBusPendingCall* pending = nullptr;

    {
        DBusMessageIter iter;
        dbus_message_iter_init_append(msg.get(), &iter);

        encode(iter, iface());
    }

    dbus_connection_send_with_reply(conn(), msg.get(), &pending, TimeoutRAIIHelper(disp()));

    return getall_properties_holder_type(PendingCall(dbus_message_get_serial(msg.get()), pending), *this);
}


PendingCall StubBase::send_request(ClientMethodBase* method, std::function<void(DBusMessageIter&)>&& f, bool is_oneway)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), iface(), method->method_name_));
    DBusPendingCall* pending = nullptr;

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);

    f(iter);

    if (!is_oneway)
    {
        dbus_connection_send_with_reply(disp().conn_, msg.get(), &pending, TimeoutRAIIHelper(disp()));
    }
    else
    {
       // otherwise server would stop reading requests after a while
       dbus_message_set_no_reply(msg.get(), TRUE);

       dbus_connection_send(disp().conn_, msg.get(), nullptr);
       dbus_connection_flush(disp().conn_);
    }

    return PendingCall(dbus_message_get_serial(msg.get()), pending);
}


message_ptr_t StubBase::send_request_and_block(ClientMethodBase* method, std::function<void(DBusMessageIter&)>&& f, bool is_oneway)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), iface(), method->method_name_));
    DBusPendingCall* pending = nullptr;
    message_ptr_t rc(nullptr, &dbus_message_unref);

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);

    f(iter);

    if (!is_oneway)
    {
        dbus_connection_send_with_reply(disp().conn_, msg.get(), &pending, TimeoutRAIIHelper(disp()));

        dbus_pending_call_block(pending);

        rc = make_message(dbus_pending_call_steal_reply(pending));
        dbus_pending_call_unref(pending);

        if (dbus_message_get_type(rc.get()) == DBUS_MESSAGE_TYPE_ERROR)
            method->_throw(*rc);
    }
    else
    {
       // otherwise server would stop reading requests after a while
       dbus_message_set_no_reply(msg.get(), TRUE);

       dbus_connection_send(disp().conn_, msg.get(), nullptr);
       dbus_connection_flush(disp().conn_);
    }

    return rc;
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

   auto sig = signals_;
   while(sig)
   {
      // already attached?
      if (&sigbase == sig)
         return;

      sig = sig->next_;
   }

   disp_->register_signal(*this, sigbase);

   sigbase.next_ = signals_;
   signals_ = &sigbase;
}


void StubBase::unregister_signal(ClientSignalBase& sigbase)
{
   assert(disp_);

   ClientSignalBase* last = nullptr;
   auto sig = signals_;

   while(sig)
   {
      // found...
      if (&sigbase == sig)
      {
         disp_->unregister_signal(*this, sigbase);

         // remove from list
         if (last)
         {
            last->next_ = sig->next_;
         }
         else
            signals_ = sig->next_;

         sigbase.next_ = nullptr;
         break;
      }

      last = sig;
      sig = sig->next_;
   }
}


void StubBase::attach_property(ClientPropertyBase* prop)
{
   assert(disp_);

   auto propiter = std::find_if(properties_.begin(), properties_.end(), [prop](auto& pair){ return prop == pair.first; });

   if (propiter->second == false)
   {
      propiter->second = true;

      if (++attached_properties_ == 1)
         disp_->register_properties(*this);
   }
}


void StubBase::detach_property(ClientPropertyBase* prop)
{
   assert(disp_);


   auto propiter = std::find_if(properties_.begin(), properties_.end(), [prop](auto& pair){ return prop == pair.first; });

   if (propiter->second)
   {
      propiter->second = false;

      if (--attached_properties_ == 0)
        disp_->unregister_properties(*this);
   }
}


void StubBase::cleanup()
{
   // cleanup signal registrations
   auto sig = signals_;
   while(sig)
   {
      disp_->unregister_signal(*this, *sig);
      sig = sig->next_;
   }

   signals_ = nullptr;

   // cleanup property registration
   if (attached_properties_)
      disp_->unregister_properties(*this);

   attached_properties_ = 0;

   disp_ = nullptr;
}


PendingCall StubBase::get_property_async(const char* name)
{
   message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Get"));
   DBusPendingCall* pending = nullptr;

   DBusMessageIter iter;
   dbus_message_iter_init_append(msg.get(), &iter);

   encode(iter, iface(), name);

    // TODO request specific timeout handling here
   dbus_connection_send_with_reply(conn(), msg.get(), &pending, disp().request_timeout());

   return PendingCall(dbus_message_get_serial(msg.get()), pending);
}


message_ptr_t StubBase::get_property(const char* name)
{
   message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Get"));

   DBusMessageIter iter;
   dbus_message_iter_init_append(msg.get(), &iter);

   encode(iter, iface(), name);

   DBusError err;
   dbus_error_init(&err);

   // TODO request specific timeout handling here
   DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn(), msg.get(), disp().request_timeout(), &err);

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

   return msg;
}


void StubBase::set_property(const char* name, std::function<void(DBusMessageIter&)>&& f)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Set"));

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);

    encode(iter, iface(), name);

    f(iter);   // and now serialize the variant

     DBusError err;
     dbus_error_init(&err);

     // TODO request specific timeout handling here
     DBusMessage* reply = dbus_connection_send_with_reply_and_block(disp().conn_, msg.get(), disp().request_timeout(), &err);

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


PendingCall StubBase::set_property_async(const char* name, std::function<void(DBusMessageIter&)>&& f)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Set"));

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);

    encode(iter, iface(), name);
    f(iter);   // and now serialize the variant

    DBusPendingCall* pending = nullptr;

    // TODO request specific timeout handling here
    dbus_connection_send_with_reply(disp().conn_, msg.get(), &pending, disp().request_timeout());

    return PendingCall(dbus_message_get_serial(msg.get()), pending);
}


void StubBase::try_handle_signal(DBusMessage* msg)
{
   // FIXME better check -> include interface!
   if (!strcmp(dbus_message_get_member(msg), "PropertiesChanged"))
   {
      DBusMessageIter it;
      dbus_message_iter_init(msg, &it);

      std::string iface;
      decode(it, iface);
      // ignore interface name for now

      DBusMessageIter iter;
      dbus_message_iter_recurse(&it, &iter);

      while(dbus_message_iter_get_arg_type(&iter) != 0)
      {
         DBusMessageIter item_iterator;
         dbus_message_iter_recurse(&iter, &item_iterator);

         std::string property_name;
         decode(item_iterator, property_name);

         auto propiter = std::find_if(properties_.begin(), properties_.end(), [&property_name](auto& pair){ return property_name == pair.first->name_; });

         if (propiter != properties_.end() && propiter->second)
            propiter->first->eval(&item_iterator);

         // advance to next element
         dbus_message_iter_next(&iter);
      }

      // check for invalidated properties
      dbus_message_iter_next(&it);
      dbus_message_iter_recurse(&it, &iter);

      while(dbus_message_iter_get_arg_type(&iter) != 0)
      {
         std::string property_name;
         decode(iter, property_name);

         auto propiter = std::find_if(properties_.begin(), properties_.end(), [&property_name](auto& pair){ return property_name == pair.first->name_; });

         if (propiter != properties_.end() && propiter->second)
            propiter->first->eval(nullptr);

         // advance to next element
         dbus_message_iter_next(&iter);
      }
   }
   else
   {
      auto sig = signals_;

      while(sig)
      {
         if (!strcmp(sig->name_, dbus_message_get_member(msg)))
         {
            DBusMessageIter iter;
            dbus_message_iter_init(msg, &iter);

            sig->eval(iter);
            break;
         }

         sig = sig->next_;
      }
   }
}


}   // namespace dbus

}   // namespace simppl

