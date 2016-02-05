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

namespace ipc
{

StubBase::StubBase(const char* iface, const char* role)
 : iface_(detail::extract_interface(iface))
 , role_(nullptr)
 , objectpath_(nullptr)
 , conn_state_(ConnectionState::Disconnected)
 , disp_(nullptr)
{
   assert(role);
   std::tie(objectpath_, role_) = detail::create_objectpath(iface_, role);
   
   busname_.reserve(strlen(this->iface()) + 1 + strlen(this->role()));
   busname_ = this->iface();
   busname_ += ".";
   busname_ += this->role();
}


StubBase::~StubBase()
{
   if (disp_)
      disp_->removeClient(*this);
      
   delete[] iface_;
   delete[] objectpath_;
   role_ = nullptr;
}


/*static*/
void StubBase::pending_notify(DBusPendingCall* pc, void* user_data)
{
    DBusMessage* msg = dbus_pending_call_steal_reply(pc);

    ClientResponseBase* handler = (ClientResponseBase*)user_data;
    handler->eval(*msg);
    
    dbus_message_unref(msg);
    dbus_pending_call_unref(pc);
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
       throw TransportError(ETIMEDOUT, -1 /* FIXME what's that sequence nr for */);
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


DBusPendingCall* StubBase::sendRequest(ClientRequestBase& req, std::function<void(detail::Serializer&)> f)
{
    DBusMessage* msg = dbus_message_new_method_call(boundname().c_str(), objectpath(), iface(), req.method_name_);
    DBusPendingCall* pending = nullptr;

    detail::Serializer s(msg);
    f(s);

    if (req.handler_)
    {
        int timeout = disp().request_timeout();

        if (detail::request_specific_timeout.count() > 0)
            timeout = detail::request_specific_timeout.count();

        dbus_connection_send_with_reply(disp().conn_, msg, &pending, timeout);
        dbus_pending_call_set_notify(pending, &StubBase::pending_notify, req.handler_, 0);
    }
    else
       dbus_connection_send(disp().conn_, msg, nullptr);

    dbus_message_unref(msg);
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
   DBusMessage* msg = dbus_message_new_method_call(boundname().c_str(), objectpath(), "org.freedesktop.DBus.Properties", "Get");
   DBusPendingCall* pending = nullptr;

   detail::Serializer s(msg);
   s << iface();
   s << name;

   dbus_connection_send_with_reply(conn(), msg, &pending, -1);
   dbus_message_unref(msg);

   dbus_pending_call_set_notify(pending, callback, user_data, 0);
}


void StubBase::try_handle_signal(DBusMessage* msg)
{
   auto iter = signals_.find(dbus_message_get_member(msg));
   if (iter != signals_.end())
   {
       iter->second->eval(msg);
   }
}


}   // namespace ipc

}   // namespace simppl

