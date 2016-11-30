#include "simppl/stub.h"

#include "simppl/dispatcher.h"

#include "simppl/detail/util.h"
#include "simppl/clientside.h"

#include <cstring>
#include <chrono>
#include <cassert>


using namespace std::literals::chrono_literals;


namespace {

    struct PendingAttributeSetHelper
    {
        PendingAttributeSetHelper(const std::function<void(simppl::dbus::CallState cs)>& cb)
         : cb_(cb)
        {
            // NOOP
        }

        static
        void _delete(void* user_data)
        {
            PendingAttributeSetHelper* that = static_cast<PendingAttributeSetHelper*>(user_data);
            delete that;
        }

        static
        void callback(DBusPendingCall* pending, void *user_data)
        {
            simppl::dbus::dbus_pending_call_ptr_t upc = simppl::dbus::make_pending_call(pending);
            simppl::dbus::dbus_message_ptr_t msg = simppl::dbus::make_message(dbus_pending_call_steal_reply(pending));

            simppl::dbus::CallState cs(*msg);
            PendingAttributeSetHelper* that = static_cast<PendingAttributeSetHelper*>(user_data);
            that->cb_(cs);
        }

        std::function<void(simppl::dbus::CallState cs)> cb_;
    };

}   // namespace


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
      disp_->removeClient(*this);

   delete[] iface_;
   delete[] objectpath_;
}


void StubBase::connect()
{
    std::chrono::milliseconds sum = 0ms;
    auto finish = std::chrono::steady_clock::now();

    if (disp().request_timeout() > 0)
    {
        finish += std::chrono::milliseconds(disp().request_timeout());
    }
    else
        finish = std::chrono::steady_clock::time_point::max();

    while(!is_connected() && std::chrono::steady_clock::now() <= finish)
    {
        if (std::chrono::steady_clock::now() < finish)
        {
            disp().step(50ms);
            sum += 50ms;
        }
    }

    if (!is_connected())
       throw Error(DBUS_ERROR_TIMEOUT);
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


DBusPendingCall* StubBase::sendRequest(const char* method_name, std::function<void(detail::Serializer&)> f, bool is_oneway)
{
    dbus_message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), iface(), method_name));
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


void StubBase::sendSignalRegistration(ClientSignalBase& sigbase)
{
   assert(disp_);
   assert(signals_.find(sigbase.name()) == signals_.end());   // signal already attached by this stub

   disp_->registerSignal(*this, sigbase);

   // FIXME use intrusive container and iterate instead of using a map -> memory efficiency
   signals_[sigbase.name()] = &sigbase;
}


void StubBase::sendSignalUnregistration(ClientSignalBase& sigbase)
{
   assert(disp_);

   auto iter = signals_.find(sigbase.name());

   if (iter != signals_.end())
   {
      disp_->unregisterSignal(*this, sigbase);
      signals_.erase(iter);
   }
}


void StubBase::cleanup()
{
   for (auto& sig_entry : signals_)
   {
      disp_->unregisterSignal(*this, *sig_entry.second);
   }

   signals_.clear();

   disp_ = nullptr;
}


void StubBase::getProperty(const char* name, void(*callback)(DBusPendingCall*, void*), void* user_data)
{
   dbus_message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Get"));
   DBusPendingCall* pending = nullptr;

   detail::Serializer s(msg.get());
   s << iface() << name;

   dbus_connection_send_with_reply(conn(), msg.get(), &pending, -1);

   dbus_pending_call_set_notify(pending, callback, user_data, 0);
}


dbus_message_ptr_t StubBase::getProperty(const char* name)
{
   dbus_message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Get"));
   DBusPendingCall* pending = nullptr;

   detail::Serializer s(msg.get());
   s << iface() << name;

   dbus_connection_send_with_reply(conn(), msg.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT);

   dbus_pending_call_block(pending);

   msg.reset(dbus_pending_call_steal_reply(pending));
   dbus_pending_call_unref(pending);

   return msg;
}


void StubBase::setProperty(const char* name, std::function<void(detail::Serializer&)> f, const std::function<void(CallState)>* callback)
{
    dbus_message_ptr_t msg = make_message(dbus_message_new_method_call(busname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Set"));

    detail::Serializer s(msg.get());
    s << iface() << name;
    f(s);   // and now serialize the variant

    if (callback)
    {
        DBusPendingCall* pending = nullptr;

        dbus_connection_send_with_reply(disp().conn_, msg.get(), &pending, DBUS_TIMEOUT_USE_DEFAULT);

        PendingAttributeSetHelper* helper = new PendingAttributeSetHelper(*callback);
        dbus_pending_call_set_notify(pending, &PendingAttributeSetHelper::callback, helper, &PendingAttributeSetHelper::_delete);
    }
    else
    {
        DBusError err;
        dbus_error_init(&err);

        DBusMessage* reply = dbus_connection_send_with_reply_and_block(disp().conn_, msg.get(), DBUS_TIMEOUT_USE_DEFAULT, &err);

        // drop original message
        msg.reset();

        // check for reponse
        if (dbus_error_is_set(&err))
        {
           Error ex(err.name, err.message);

           dbus_error_free(&err);
           throw ex;
        }
    }
}


void StubBase::try_handle_signal(DBusMessage* msg)
{
   auto iter = signals_.find(dbus_message_get_member(msg));
   if (iter != signals_.end())
   {
       iter->second->eval(msg);
   }
}


}   // namespace dbus

}   // namespace simppl

