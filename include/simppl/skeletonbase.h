#ifndef SIMPPL_SKELETONBASE_H
#define SIMPPL_SKELETONBASE_H

#include <tuple>

#include <dbus/dbus.h>

#include <iostream>  // FIXME remove this

#include "simppl/error.h"
#include "simppl/serverrequestdescriptor.h"

#include "simppl/detail/serverresponseholder.h"


namespace simppl
{

namespace ipc
{

// forward decl
struct Dispatcher;


struct SkeletonBase
{
   SkeletonBase(const SkeletonBase&) = delete;
   SkeletonBase& operator=(const SkeletonBase&) = delete;
   
   friend struct Dispatcher;

   static DBusHandlerResult method_handler(DBusConnection *connection, DBusMessage *message, void *user_data);

   SkeletonBase(const char* iface, const char* role);

   virtual ~SkeletonBase();

   Dispatcher& disp();

   /// only valid within request handler - must be called in order to respond to the request later in time
   ServerRequestDescriptor deferResponse();

   /// only valid to call within request handler
   void respondWith(detail::ServerResponseHolder response);

   /// send deferred response as retrieved by calling deferResponse()
   void respondOn(ServerRequestDescriptor& req, detail::ServerResponseHolder response);

   /// send error response - only valid to call within request handler
   void respondWith(const RuntimeError& err);

   /// send deferred error response as retrieved by calling deferResponse()
   void respondOn(ServerRequestDescriptor& req, const RuntimeError& err);

   const ServerRequestDescriptor& currentRequest() const;

   inline
   const char* iface() const
   {
      return iface_;
   }

   inline
   const char* role() const
   {
      return role_;
   }

   inline
   const char* objectpath() const
   {
      return objectpath_;
   }
   

protected:

   DBusHandlerResult handleRequest(DBusMessage* msg);

   /// return a session pointer and destruction function if adequate
   ///virtual std::tuple<void*,void(*)(void*)> clientAttached();

   char* iface_;
   char* role_;
   char* objectpath_;

   Dispatcher* disp_;
   ServerRequestDescriptor current_request_;
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_SKELETONBASE_H
