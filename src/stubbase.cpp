#include "simppl/stub.h"

#include "simppl/dispatcher.h"

#include "simppl/detail/frames.h"
#include "simppl/detail/util.h"
#include "simppl/clientside.h"

#include <cstring>
#include <thread>
#include <chrono>
#include <sstream>
#include <cassert>


using namespace std::literals::chrono_literals;


namespace simppl
{

namespace ipc
{


StubBase::StubBase(const char* iface, const char* role)
 : role_(role)
 , conn_state_(ConnectionState::Disconnected)
 , disp_(0)
{
   assert(iface);
   assert(role);

   // FIXME move this code to helper global function

   // strip template arguments
   memset(iface_, 0, sizeof(iface_));
   strncpy(iface_, iface, strstr(iface, "<") - iface);

   // remove '::' separation in favour of '.' separation
   char *readp = iface_, *writep = iface_;
   while(*readp)
   {
      if (*readp == ':')
      {
         *writep++ = '.';
         readp += 2;
      }
      else
         *writep++ = *readp++;
   }

   // terminate
   *writep = '\0';
}


StubBase::~StubBase()
{
   if (disp_)
      disp_->removeClient(*this);
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
    DBusMessage* msg = dbus_message_new_method_call(boundname().c_str(), objectpath().c_str(), iface(), req.method_name_);
    DBusPendingCall* pending = nullptr;

    detail::Serializer s(msg);
    f(s);

    if (req.handler_)
    {
        int timeout = disp().request_timeout();

        if (detail::request_specific_timeout.count() > 0)
            timeout = detail::request_specific_timeout.count();

        dbus_connection_send_with_reply(disp().conn_, msg, &pending, timeout);
        dbus_pending_call_set_notify(pending, &ClientResponseBase::pending_notify, req.handler_, 0);
    }
    else
       dbus_connection_send(disp().conn_, msg, nullptr);

    dbus_message_unref(msg);
    detail::request_specific_timeout = std::chrono::milliseconds(0);

    return pending;
}


std::string StubBase::boundname() const
{ 
    // FIXME optimize, use C string only and cache result?!
    std::ostringstream bname;
    bname << iface_ << "." << role_;

    return bname.str();
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
   DBusMessage* msg = dbus_message_new_method_call(boundname().c_str(), objectpath().c_str(), "org.freedesktop.DBus.Properties", "Get");
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
   const char *method = dbus_message_get_member(msg);

   auto iter = signals_.find(dbus_message_get_member(msg));
   if (iter != signals_.end())
   {
       iter->second->eval(msg);
   }
}


}   // namespace ipc

}   // namespace simppl

