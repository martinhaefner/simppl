#include "simppl/stub.h"

#include "simppl/dispatcher.h"
#include "simppl/string.h"

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

StubBase::StubBase()
 : iface_(nullptr)
 , objectpath_(nullptr)
 , conn_state_(ConnectionState::Disconnected)
 , disp_(nullptr)
 , signals_(nullptr)
 , properties_(nullptr)
{
    // NOOP
}


StubBase::~StubBase()
{
   if (disp_)
      disp_->remove_client(*this);

   delete[] iface_;
   delete[] objectpath_;
}


void StubBase::init(char* iface, const char* busname, const char* objectpath)
{
    assert(busname);
    assert(objectpath);

    iface_ = detail::extract_interface(iface);

    objectpath_ = new char[strlen(objectpath)+1];
    strcpy(objectpath_, objectpath);

    busname_ = busname;

    free(iface);
}


void StubBase::init(char* iface, const char* role)
{
    assert(role);

    iface_ = detail::extract_interface(iface);

    objectpath_ = detail::create_objectpath(iface_, role);

    busname_.reserve(strlen(this->iface()) + 1 + strlen(role));
    busname_ = this->iface();
    busname_ += ".";
    busname_ += role;

    free(iface);
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


PendingCall StubBase::send_request(const char* method_name, std::function<void(DBusMessageIter&)>&& f, bool is_oneway)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), iface(), method_name));
    DBusPendingCall* pending = nullptr;

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);
    
    f(iter);

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

    return PendingCall(dbus_message_get_serial(msg.get()), pending);
}


message_ptr_t StubBase::send_request_and_block(const char* method_name, std::function<void(DBusMessageIter&)>&& f, bool is_oneway)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), iface(), method_name));
    DBusPendingCall* pending = nullptr;
    message_ptr_t rc(nullptr, &dbus_message_unref);

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);
    
    f(iter);

    if (!is_oneway)
    {
        int timeout = disp().request_timeout();

        if (detail::request_specific_timeout.count() > 0)
            timeout = detail::request_specific_timeout.count();

        dbus_connection_send_with_reply(disp().conn_, msg.get(), &pending, timeout);

        detail::request_specific_timeout = std::chrono::milliseconds(0);

        dbus_pending_call_block(pending);

        rc = make_message(dbus_pending_call_steal_reply(pending));
        dbus_pending_call_unref(pending);

        CallState cs(*rc);

        if (!cs)
           cs.throw_exception();
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


void StubBase::attach_property(ClientPropertyBase& prop)
{
   assert(disp_);
   
   if (!properties_)
      disp_->register_properties(*this);

   auto p = properties_;
   while(p)
   {
      // already attached?
      if (&prop == p)
         return;
      
      p = p->next_;
   }
   
   prop.next_ = properties_;
   properties_ = &prop;
}


void StubBase::detach_property(ClientPropertyBase& prop)
{
   assert(disp_);

   ClientPropertyBase* last = nullptr;
   auto p = properties_;
   
   while(p)
   {
      // found...
      if (&prop == p)
      {
         // remove from list
         if (last)
         {
            last->next_ = p->next_;
         }
         else
            properties_ = p->next_;
            
         prop.next_ = nullptr;
         break;
      }
      
      last = p;
      p = p->next_;
   }
   
   // empty?
   if (!properties_)
      disp_->unregister_properties(*this);
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
   if (properties_)
      disp_->unregister_properties(*this);

   properties_ = nullptr;

   disp_ = nullptr;
}


PendingCall StubBase::get_property_async(const char* name)
{
   message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Get"));
   DBusPendingCall* pending = nullptr;

   DBusMessageIter iter;
   dbus_message_iter_init_append(msg.get(), &iter);
    
   encode(iter, iface(), name);

   dbus_connection_send_with_reply(conn(), msg.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT);

   return PendingCall(dbus_message_get_serial(msg.get()), pending);
}


message_ptr_t StubBase::get_property(const char* name)
{
   message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Get"));
   DBusPendingCall* pending = nullptr;

   DBusMessageIter iter;
   dbus_message_iter_init_append(msg.get(), &iter);

   encode(iter, iface(), name);

   dbus_connection_send_with_reply(conn(), msg.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT);

   dbus_pending_call_block(pending);

   msg.reset(dbus_pending_call_steal_reply(pending));
   dbus_pending_call_unref(pending);

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


PendingCall StubBase::set_property_async(const char* name, std::function<void(DBusMessageIter&)>&& f)
{
    message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Set"));

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);
    
    encode(iter, iface(), name);
    f(iter);   // and now serialize the variant

    DBusPendingCall* pending = nullptr;

    dbus_connection_send_with_reply(disp().conn_, msg.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT);

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

         auto p = properties_;
         while(p)
         {
            if (property_name == p->name_)
            {
               p->eval(item_iterator);
               break;
            } 
            
            p = p->next_;
         }
         
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

