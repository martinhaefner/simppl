#include "simppl/skeletonbase.h"

#include "simppl/dispatcher.h"
#include "simppl/interface.h"
#include "simppl/string.h"
#include "simppl/vector.h"

#define SIMPPL_SKELETONBASE_CPP
#include "simppl/serverside.h"
#undef SIMPPL_SKELETONBASE_CPP

#include "simppl/detail/util.h"

#include <cxxabi.h>

#include <algorithm>
#include <iostream>
#include <memory>


namespace simppl
{

namespace dbus
{

namespace detail
{

struct FreeDeleter {
    template<typename T>
    void operator()(T* o) {
        ::free(o);
    }
};

using DemangledNamePtr = std::unique_ptr<char, FreeDeleter>;

} // namespace detail


/*static*/
DBusHandlerResult SkeletonBase::method_handler(DBusConnection* connection, DBusMessage* msg, void *user_data)
{
   SkeletonBase* skel = (SkeletonBase*)user_data;
   return skel->handle_request(msg);
}


SkeletonBase::SkeletonBase(std::size_t iface_count)
  : ifaces_(iface_count, std::string())
  , disp_(nullptr)
  , method_heads_(iface_count, nullptr)
  , property_heads_(iface_count, nullptr)
#if SIMPPL_HAVE_INTROSPECTION
  , signal_heads_(iface_count, nullptr)
#endif
{}


SkeletonBase::~SkeletonBase()
{
   if (disp_)
      disp_->remove_server(*this);
}


void SkeletonBase::init(const char* mangled_iface_list, const char* role)
{
    assert(role);

    detail::DemangledNamePtr iface(abi::__cxa_demangle(mangled_iface_list, 0, 0, 0));
    ifaces_ = detail::extract_interfaces(1, iface.get());

    objectpath_ = detail::create_objectpath(ifaces_[0].c_str(), role);
    busname_ = detail::create_busname(ifaces_[0].c_str(), role);
}


void SkeletonBase::init(size_type iface_count, const char* mangled_iface_list, std::string busname, std::string objectpath)
{
    assert(busname.length() > 0);
    assert(objectpath.length() > 0);

    detail::DemangledNamePtr iface(abi::__cxa_demangle(mangled_iface_list, 0, 0, 0));
    ifaces_ = detail::extract_interfaces(iface_count, iface.get());
    busname_ = std::move(busname);
    objectpath_ = std::move(objectpath);
}


void SkeletonBase::init(std::string busname, std::string objectpath)
{
    assert(busname.length() > 0);
    assert(objectpath.length() > 0);

    busname_ = std::move(busname);
    objectpath_ = std::move(objectpath);
}


Dispatcher& SkeletonBase::disp()
{
   assert(disp_);
   return *disp_;
}


ServerRequestDescriptor SkeletonBase::defer_response()
{
   assert(current_request_);
  // assert(current_request_.requestor_->hasResponse());

   return std::move(current_request_);
}


void SkeletonBase::respond_with(detail::ServerResponseHolder response)
{
   assert(current_request_);
   //assert(response.responder_->allowedRequests_.find(current_request_.requestor_) != response.responder_->allowedRequests_.end());

   message_ptr_t rmsg = make_message(dbus_message_new_method_return(current_request_.msg_));

   if (response.f_)
   {
      DBusMessageIter iter;
      dbus_message_iter_init_append(rmsg.get(), &iter);

      response.f_(iter);
   }

   dbus_connection_send(disp_->conn_, rmsg.get(), nullptr);

   current_request_.clear();   // only respond once!!!
}


void SkeletonBase::respond_on(ServerRequestDescriptor& req, detail::ServerResponseHolder response)
{
   assert(req);
   //assert(response.responder_->allowedRequests_.find(req.requestor_) != response.responder_->allowedRequests_.end());

   message_ptr_t rmsg = make_message(dbus_message_new_method_return(req.msg_));

   if (response.f_)
   {
      DBusMessageIter iter;
      dbus_message_iter_init_append(rmsg.get(), &iter);

      response.f_(iter);
   }

   dbus_connection_send(disp_->conn_, rmsg.get(), nullptr);

   req.clear();
}


void SkeletonBase::respond_with(const Error& err)
{
   assert(current_request_);
   //assert(current_request_.requestor_->hasResponse());

   message_ptr_t rmsg = err.make_reply_for(*current_request().msg_);
   dbus_connection_send(disp_->conn_, rmsg.get(), nullptr);

   current_request_.clear();   // only respond once!!!
}


void SkeletonBase::respond_on(ServerRequestDescriptor& req, const Error& err)
{
   assert(req);
   //assert(req.requestor_->hasResponse());

   message_ptr_t rmsg = err.make_reply_for(*req.msg_);
   dbus_connection_send(disp_->conn_, rmsg.get(), nullptr);

   req.clear();
}


const ServerRequestDescriptor& SkeletonBase::current_request() const
{
   assert(current_request_);
   return current_request_;
}


DBusHandlerResult SkeletonBase::handle_request(DBusMessage* msg)
{
    const char* method_name = dbus_message_get_member(msg);
    const char* interface_name = dbus_message_get_interface(msg);

#ifdef SIMPPL_HAVE_INTROSPECTION
    if (!strcmp(interface_name, "org.freedesktop.DBus.Introspectable"))
    {
        return handle_introspect_request(msg);
    }
#endif

    if (!strcmp(interface_name, "org.freedesktop.DBus.Properties"))
    {
        return handle_property_request(msg);
    }

    int iface_id = find_interface(interface_name);
    if (iface_id == invalid_iface_id)
    {
        return handle_error(msg, DBUS_ERROR_UNKNOWN_INTERFACE);
    }

    ServerMethodBase* method = find_method(iface_id, method_name);
    if (!method)
    {
        return handle_error(msg, DBUS_ERROR_UNKNOWN_METHOD);
    }

    return handle_interface_request(msg, *method);
}


#ifdef SIMPPL_HAVE_INTROSPECTION
DBusHandlerResult SkeletonBase::handle_introspect_request(DBusMessage* msg)
{
    const char* method = dbus_message_get_member(msg);

    if (strcmp(method, "Introspect") != 0)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    std::ostringstream oss;

    oss << "<?xml version=\"1.0\" ?>\n"
           "<node name=\""<< objectpath() << "\">\n";

    for (size_type i = 0; i < ifaces_.size(); ++i)
    {
        introspect_interface(oss, i);
    }

    // introspectable
    oss << "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
           "    <method name=\"Introspect\">\n"
           "      <arg name=\"data\" type=\"s\" direction=\"out\"/>\n"
           "    </method>\n"
           "  </interface>\n";

    // properties
    if (has_any_properties())
    {
        oss << "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
               "    <method name=\"Get\">\n"
               "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
               "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
               "      <arg name=\"value\" type=\"v\" direction=\"out\"/>\n"
               "    </method>\n"
               "    <method name=\"Set\">\n"
               "      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
               "      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
               "      <arg name=\"value\" type=\"v\" direction=\"in\"/>\n"
               "    </method>\n"
               "    <signal name=\"PropertiesChanged\">\n"
               "      <arg name=\"interface_name\" type=\"s\"/>\n"
               "      <arg name=\"changed_properties\" type=\"a{sv}\"/>\n"
               "      <arg name=\"invalidated_properties\" type=\"as\"/>\n"
               "    </signal>\n"
               "  </interface>\n";
    }

    oss << "</node>\n";

    DBusMessage* reply = dbus_message_new_method_return(msg);

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);

    encode(iter, oss.str());

    dbus_connection_send(disp_->conn_, reply, nullptr);
    return DBUS_HANDLER_RESULT_HANDLED;
}
#endif  // defined(SIMPPL_HAVE_INTROSPECTION)


DBusHandlerResult SkeletonBase::handle_property_request(DBusMessage* msg)
{
    DBusMessageIter iter;
    std::string interface_name;
    std::string property_name;

    try
    {
        dbus_message_iter_init(msg, &iter);
        decode(iter, interface_name, property_name);
    }
    catch(DecoderError&)
    {
        return handle_error(msg, DBUS_ERROR_INVALID_ARGS);
    }

    int iface_id = find_interface(interface_name);
    if (iface_id == invalid_iface_id)
    {
        return handle_error(msg, DBUS_ERROR_UNKNOWN_INTERFACE);
    }

    ServerPropertyBase* property = find_property(iface_id, property_name);
    if (!property)
    {
        return handle_error(msg, DBUS_ERROR_UNKNOWN_PROPERTY);
    }

    const char* method = dbus_message_get_member(msg);
    if (!strcmp(method, "Get"))
    {
        return handle_property_get_request(msg, *property);
    }
    else if(!strcmp(method, "Set"))
    {
        return handle_property_set_request(msg, *property, iter);
    }
    else
    {
        return handle_error(msg, DBUS_ERROR_UNKNOWN_METHOD);
    }
}


DBusHandlerResult SkeletonBase::handle_property_get_request(DBusMessage* msg, ServerPropertyBase& property)
{
    message_ptr_t response = make_message(dbus_message_new_method_return(msg));
    property.eval(response.get());
    dbus_connection_send(disp_->conn_, response.get(), nullptr);
    return DBUS_HANDLER_RESULT_HANDLED;
}


DBusHandlerResult SkeletonBase::handle_property_set_request(DBusMessage* msg, ServerPropertyBase& property, DBusMessageIter& iter)
{
    message_ptr_t response = make_message(nullptr);
    try
    {
        property.evalSet(iter);
        response = make_message(dbus_message_new_method_return(msg));
    }
    catch(simppl::dbus::Error& err)
    {
        response = err.make_reply_for(*msg);
    }
    catch(...)
    {
        simppl::dbus::Error err("simppl.dbus.UnhandledException");
        response = err.make_reply_for(*msg);
    }

    dbus_connection_send(disp_->conn_, response.get(), nullptr);
    return DBUS_HANDLER_RESULT_HANDLED;
}


DBusHandlerResult SkeletonBase::handle_interface_request(DBusMessage* msg, ServerMethodBase& method)
{
    current_request_.set(&method, msg);

    try
    {
        method.eval(msg);
    }
    catch(DecoderError&)
    {
        // don't use `handle_error` as we may need to clear the current request
        simppl::dbus::Error err(DBUS_ERROR_INVALID_ARGS);
        auto r = err.make_reply_for(*msg);
        dbus_connection_send(disp_->conn_, r.get(), nullptr);
    }
    catch(...)
    {
        // don't use `handle_error` as we may need to clear the current request
        simppl::dbus::Error e("simppl.dbus.UnhandledException");
        auto r = e.make_reply_for(*msg);
        dbus_connection_send(disp_->conn_, r.get(), nullptr);
    }

    // current_request_ is only valid if no response handler was called
    if (current_request_)
        current_request_.clear();

    return DBUS_HANDLER_RESULT_HANDLED;
}


DBusHandlerResult SkeletonBase::handle_error(DBusMessage* msg, const char* dbus_error)
{
    simppl::dbus::Error err(dbus_error);
    auto r = err.make_reply_for(*msg);
    dbus_connection_send(disp_->conn_, r.get(), nullptr);
    return DBUS_HANDLER_RESULT_HANDLED;
}

#if SIMPPL_HAVE_INTROSPECTION
void SkeletonBase::introspect_interface(std::ostream& os, size_type index) const
{
    os << "  <interface name=\""<< iface(index) << "\">\n";

    for (auto pm = method_heads_[index]; pm; pm = pm->next_)
    {
        pm->introspect(os);
    }

    for (auto pp = this->property_heads_[index]; pp; pp = pp->next_)
    {
        pp->introspect(os);
    }

    for (auto ps = this->signal_heads_[index]; ps; ps = ps->next_)
    {
        ps->introspect(os);
    }

    os << "  </interface>\n";
}
#endif //

bool SkeletonBase::has_any_properties() const
{
    const auto first = property_heads_.begin();
    const auto last = property_heads_.end();
    return std::find_if(first, last, [](ServerPropertyBase* ptr) { return ptr != nullptr; }) != last;
}


int SkeletonBase::find_interface(const char* name) const
{
    for (size_type i = 0; i < ifaces_.size(); ++i)
    {
        // use strcmp() because ifaces_[i] contains trailing zeros which causes
        // operator== to fail.
        if (!strcmp(ifaces_[i].c_str(), name))
        {
            // `ifaces_[0]` has the highest ID (e.g. N-1), so we need to
            // "reverse" the order.
            return static_cast<int>(ifaces_.size() - i - 1);
        }
    }

    return invalid_iface_id;
}


ServerPropertyBase* SkeletonBase::find_property(int iface_id, const char* name) const
{
    for (auto pp = property_heads_[iface_id]; pp; pp = pp->next_)
    {
        if (!strcmp(pp->name_, name))
        {
            return pp;
        }
    }

    return nullptr;
}


ServerMethodBase* SkeletonBase::find_method(int iface_id, const char* name) const
{
    for (auto pm = method_heads_[iface_id]; pm; pm = pm->next_)
    {
        if (!strcmp(pm->name_, name))
        {
            return pm;
        }
    }

    return nullptr;
}


void SkeletonBase::send_signal(const char* signame, int iface_id, std::function<void(DBusMessageIter&)>&& f)
{
    message_ptr_t msg = make_message(dbus_message_new_signal(objectpath(), iface(iface_id).c_str(), signame));

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);

    f(iter);

    dbus_connection_send(disp_->conn_, msg.get(), nullptr);
}


void SkeletonBase::send_property_change(const char* prop, int iface_id, std::function<void(DBusMessageIter&)>&& f)
{
   static std::vector<std::string> invalid;

   message_ptr_t msg = make_message(dbus_message_new_signal(objectpath(), "org.freedesktop.DBus.Properties", "PropertiesChanged"));

   DBusMessageIter iter;
   dbus_message_iter_init_append(msg.get(), &iter);

   encode(iter, iface(iface_id));

   DBusMessageIter vec_iter;
   dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
      DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING DBUS_DICT_ENTRY_END_CHAR_AS_STRING
      , &vec_iter);

   DBusMessageIter item_iterator;
   dbus_message_iter_open_container(&vec_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &item_iterator);

   encode(item_iterator, prop);
   f(item_iterator);

   // the dict entry
   dbus_message_iter_close_container(&vec_iter, &item_iterator);

   // the map
   dbus_message_iter_close_container(&iter, &vec_iter);
   encode(iter, invalid);

   dbus_connection_send(disp_->conn_, msg.get(), nullptr);
}


}   // namespace dbus

}   // namespace simppl
