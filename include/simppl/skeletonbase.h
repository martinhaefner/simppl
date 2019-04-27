#ifndef SIMPPL_SKELETONBASE_H
#define SIMPPL_SKELETONBASE_H

#include <string>
#include <vector>

#include <dbus/dbus.h>

#include "simppl/error.h"
#include "simppl/serverrequestdescriptor.h"

#include "simppl/detail/serverresponseholder.h"


namespace simppl
{

namespace dbus
{

// forward decl
struct Dispatcher;
struct ServerMethodBase;
struct ServerPropertyBase;
struct ServerSignalBase;


struct SkeletonBase
{
    using size_type = std::vector<std::string>::size_type;

    SkeletonBase(const SkeletonBase&) = delete;
    SkeletonBase& operator=(const SkeletonBase&) = delete;

    friend struct Dispatcher;
    friend struct ServerMethodBase;
    friend struct ServerPropertyBase;
    friend struct ServerSignalBase;

    static DBusHandlerResult method_handler(DBusConnection *connection, DBusMessage *message, void *user_data);

    SkeletonBase(std::size_t iface_count);

    virtual ~SkeletonBase();

    Dispatcher& disp();

    /// only valid within request handler - must be called in order to respond to the request later in time
    ServerRequestDescriptor defer_response();

    /// only valid to call within request handler
    void respond_with(detail::ServerResponseHolder response);

    /// send deferred response as retrieved by calling deferResponse()
    void respond_on(ServerRequestDescriptor& req, detail::ServerResponseHolder response);

    /// send error response - only valid to call within request handler
    void respond_with(const Error& err);

    /// send deferred error response as retrieved by calling deferResponse()
    void respond_on(ServerRequestDescriptor& req, const Error& err);

    const ServerRequestDescriptor& current_request() const;

    inline
    const std::string& iface(size_type iface_id) const
    {
        // `ifaces_[0]` has the highest ID (e.g. N-1).
        return ifaces_[ifaces_.size() - iface_id - 1];
    }

    inline
    const char* objectpath() const
    {
        return objectpath_.c_str();
    }

    inline
    const char* busname() const
    {
        return busname_.c_str();
    }

    void send_property_change(const char* prop, int iface_id, std::function<void(DBusMessageIter&)>&& f);
    void send_signal(const char* signame, int iface_id, std::function<void(DBusMessageIter&)>&& f);

protected:
    static constexpr int invalid_iface_id = -1;

    void init(const char* mangled_iface_list, const char* role);
    void init(size_type iface_count, const char* mangled_iface_list, std::string busname, std::string objectpath);
    void init(std::string busname, std::string objectpath);

    DBusHandlerResult handle_request(DBusMessage* msg);
#if SIMPPL_HAVE_INTROSPECTION
    DBusHandlerResult handle_introspect_request(DBusMessage* msg);
#endif
    DBusHandlerResult handle_property_request(DBusMessage* msg);
    DBusHandlerResult handle_property_get_request(DBusMessage* msg, ServerPropertyBase& property);
    DBusHandlerResult handle_property_set_request(DBusMessage* msg, ServerPropertyBase& property, DBusMessageIter& iter);
    DBusHandlerResult handle_interface_request(DBusMessage* msg, ServerMethodBase& method);
    DBusHandlerResult handle_error(DBusMessage* msg, const char* dbus_error);

#if SIMPPL_HAVE_INTROSPECTION
    void introspect_interface(std::ostream& os, size_type index) const;
#endif    
    bool has_any_properties() const;
    ServerPropertyBase* find_property(int iface_id, const char* name) const;
    ServerMethodBase* find_method(int iface_id, const char* name) const;
    int find_interface(const char* name) const;

    inline
    int find_interface(const std::string& name) const
    {
        return find_interface(name.c_str());
    }

    inline
    ServerPropertyBase* find_property(int iface_id, const std::string& name) const
    {
        return find_property(iface_id, name.c_str());
    }

    inline
    ServerMethodBase* find_method(int iface_id, const std::string& name) const
    {
        return find_method(iface_id, name.c_str());
    }

    /// return a session pointer and destruction function if adequate
    ///virtual std::tuple<void*,void(*)(void*)> clientAttached();

    std::vector<std::string> ifaces_;
    std::string busname_;
    std::string objectpath_;

    Dispatcher* disp_;
    ServerRequestDescriptor current_request_;

    // linked list heads
    std::vector<ServerMethodBase*> method_heads_;
    std::vector<ServerPropertyBase*> property_heads_;

#if SIMPPL_HAVE_INTROSPECTION
    std::vector<ServerSignalBase*> signal_heads_;
#endif
};


namespace detail
{

template<std::size_t N>
struct SizedSkeletonBase : SkeletonBase {
    SizedSkeletonBase() : SkeletonBase(N) {}
};

}   // namespace detail


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_SKELETONBASE_H
