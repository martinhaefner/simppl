#include "simppl/skeletonbase.h"

#include <sstream>

#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#define SIMPPL_SKELETONBASE_CPP
#include "simppl/serverside.h"
#undef SIMPPL_SKELETONBASE_CPP

#include "simppl/detail/util.h"
#include "simppl/detail/frames.h"


namespace simppl
{
   
namespace ipc
{


/*static*/ 
DBusHandlerResult SkeletonBase::method_handler(DBusConnection* connection, DBusMessage* msg, void *user_data)
{
   SkeletonBase* skel = (SkeletonBase*)user_data;
   return skel->handleRequest(msg);
}


SkeletonBase::SkeletonBase(const char* iface, const char* role)
 : role_(role)
 , disp_(nullptr)
{
   assert(role_);
   
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


SkeletonBase::~SkeletonBase()
{
   // NOOP
}


Dispatcher& SkeletonBase::disp()
{
   assert(disp_);
   return *disp_;
}


ServerRequestDescriptor SkeletonBase::deferResponse()
{
   assert(current_request_);
   assert(current_request_.requestor_->hasResponse());  
   
   return std::move(current_request_);
}


void SkeletonBase::respondWith(detail::ServerResponseHolder response)
{
   assert(current_request_);
   assert(response.responder_->allowedRequests_.find(current_request_.requestor_) != response.responder_->allowedRequests_.end());
   
   std::cout << "Send response" << std::endl;
   if (!dbus_connection_send(disp_->conn_, response.response_, nullptr))
      std::cerr << "Failed sending response" << std::endl;
   /*
   detail::ResponseFrame r(0);
   r.payloadsize_ = response.size_;
   r.sequence_nr_ = current_request_.sequence_nr_;
   
   detail::genericSend(current_request_.fd_, r, response.payload_);*/
   current_request_.clear();   // only respond once!!!
}


void SkeletonBase::respondOn(ServerRequestDescriptor& req, detail::ServerResponseHolder response)
{
   assert(req);
   assert(response.responder_->allowedRequests_.find(req.requestor_) != response.responder_->allowedRequests_.end());
   /*
   detail::ResponseFrame r(0);
   r.payloadsize_ = response.size_;
   r.sequence_nr_ = req.sequence_nr_;
   
   genericSend(req.fd_, r, response.payload_);
   req.clear();*/
}


void SkeletonBase::respondWith(const RuntimeError& err)
{
   assert(current_request_);
   assert(current_request_.requestor_->hasResponse());
   /*
   detail::ResponseFrame r(err.error());
   r.payloadsize_ = err.what() ? strlen(err.what()) + 1 : 0;
   r.sequence_nr_ = current_request_.sequence_nr_;
   
   genericSend(current_request_.fd_, r, err.what());*/
   current_request_.clear();   // only respond once!!!
}


void SkeletonBase::respondOn(ServerRequestDescriptor& req, const RuntimeError& err)
{
   assert(req);
   assert(req.requestor_->hasResponse());
   /*
   detail::ResponseFrame r(err.error());
   r.payloadsize_ = err.what() ? strlen(err.what()) + 1 : 0;
   r.sequence_nr_ = req.sequence_nr_;
   
   genericSend(req.fd_, r, err.what());*/
   req.clear();
}


const ServerRequestDescriptor& SkeletonBase::currentRequest() const
{
   assert(current_request_);
   return current_request_;
}


DBusHandlerResult SkeletonBase::handleRequest(DBusMessage* msg)
{
   const char* method = dbus_message_get_member(msg);
   const char* interface = dbus_message_get_interface(msg);
   
   if (!strcmp(interface, "org.freedesktop.DBus.Introspectable"))
   {
      if (!strcmp(method, "Introspect"))
      {
         static char* buf = new char[512];
         
         sprintf(buf, "<?xml version=\"1.0\" ?>"
             "<node name=\"%s\">"
             "<interface name=\"%s\"></interface>"
             "<interface name=\"org.freedesktop.DBus.Introspectable\"></interface>"
             "<interface name=\"org.freedesktop.DBus.Properties\"></interface>"
             "</node>", role(), iface());

         DBusMessage* reply = dbus_message_new_method_return(msg);
         
         DBusMessageIter args;
         dbus_message_iter_init_append(reply, &args);
         
         dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &buf);
         
         dbus_connection_send(disp_->conn_, reply, nullptr);
         
         return DBUS_HANDLER_RESULT_HANDLED;
      }
   }
   else if (!strcmp(interface, "org.freedesktop.DBus.Properties"))
   {
      std::cout << "FIXME implement property handler here" << std::endl;
    
      auto& attributes = dynamic_cast<InterfaceBase<ServerRequest>*>(this)->attributes_;
      
      // FIXME read out first argument -> interface name (probably unused here)
      // FIXME read out second argument -> property name
      char* attribute = 0;
      
      auto iter = attributes.find(attribute);
      if (iter != attributes.end())
      {
         iter->second->eval(msg);
         
         return DBUS_HANDLER_RESULT_HANDLED;
      }
      else
         std::cout << "attribute '" << attribute << "' unknown" << std::endl;
   }
   else
   {
      auto& methods = dynamic_cast<InterfaceBase<ServerRequest>*>(this)->methods_;

      auto iter = methods.find(method);
      if (iter != methods.end())
      {
          current_request_.set(iter->second, msg);
            
          iter->second->eval(msg);
          
          // current_request_ is only valid if no response handler was called
          if (current_request_)
          {
             // in that case the request must not have a reponse
             assert(!current_request_.requestor_->hasResponse());  
             current_request_.clear();
          }
         
          return DBUS_HANDLER_RESULT_HANDLED;
      }
      else
         std::cout << "method '" << method << "' unknown" << std::endl;
   }
   
   return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

   //std::cout << "Skeleton::handleRequest '" << funcid << "'" << std::endl;
   /*std::map<uint32_t, ServerRequestBase*>::iterator iter;
   
   if (find(funcid, iter))
   {
      try
      {
         current_request_.set(iter->second, fd, sequence_nr, sessionid);
         iter->second->eval(payload, length);
         
         // current_request_ is only valid if no response handler was called
         if (current_request_)
         {
             // in that case the request must not have a reponse
            assert(!current_request_.requestor_->hasResponse());  
            current_request_.clear();
         }
      }
      catch(const RuntimeError& err)
      {
         if (current_request_.requestor_->hasResponse())
         {
            respondWith(err);
            current_request_.clear();
         }
      }
      catch(...)
      {
         if (current_request_.requestor_->hasResponse())
         {
            RuntimeError err(-1, "Unknown unhandled exception occured on server");
            respondWith(err);
         }
            
         current_request_.clear();
         throw;
      }
   }
   else
      std::cerr << "Unknown request '" << funcid << "' with payload size=" << length << std::endl;
      */
}

/*
std::tuple<void*,void(*)(void*)> SkeletonBase::clientAttached()
{
   return std::tuple<void*,void(*)(void*)>(nullptr, nullptr);   // the default does not create any session data
}
*/
}   // namespace ipc

}   // namespace simppl
