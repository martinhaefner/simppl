#ifndef SIMPPL_SKELETONBASE_H
#define SIMPPL_SKELETONBASE_H

#include <tuple>

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
    SkeletonBase(const SkeletonBase&) = delete;
    SkeletonBase& operator=(const SkeletonBase&) = delete;

    friend struct Dispatcher;
    friend struct ServerMethodBase;
    friend struct ServerPropertyBase;
    friend struct ServerSignalBase;

    static DBusHandlerResult method_handler(DBusConnection *connection, DBusMessage *message, void *user_data);

    SkeletonBase();

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
    const char* iface() const
    {
        return iface_;
    }

    inline
    const char* objectpath() const
    {
        return objectpath_;
    }

    inline
    const char* busname() const
    {
        return busname_;
    }

    void send_property_change(const char* prop, std::function<void(DBusMessageIter&)>&& f);

    void send_signal(const char* signame, std::function<void(DBusMessageIter&)>&& f);

protected:

    void init(char* iface, const char* role);
    void init(char* iface, const char* busname, const char* objectpath);

    DBusHandlerResult handle_request(DBusMessage* msg);

    /// return a session pointer and destruction function if adequate
    ///virtual std::tuple<void*,void(*)(void*)> clientAttached();

    char* iface_;
    char* busname_;
    char* objectpath_;

    Dispatcher* disp_;
    ServerRequestDescriptor current_request_;

    // linked list heads
    ServerMethodBase* methods_;
    ServerPropertyBase* properties_;

#if SIMPPL_HAVE_INTROSPECTION
    ServerSignalBase* signals_;
#endif
};

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_SKELETONBASE_H
