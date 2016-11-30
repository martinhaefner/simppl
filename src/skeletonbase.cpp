#include "simppl/skeletonbase.h"

#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#define SIMPPL_SKELETONBASE_CPP
#include "simppl/serverside.h"
#undef SIMPPL_SKELETONBASE_CPP

#include "simppl/detail/util.h"


#if 0
namespace org
{
   namespace freedesktop
   {
      namespace DBus
      {
         INTERFACE(Introspectable)
         {
            //typedef tTrueType introspectable;

            Request<> Introspect;
            Response<std::string> rIntrospect;

            Introspectable()
             : INIT(Introspect)
             , INIT(rIntrospect)
            {
               Introspect >> rIntrospect;

               //describe_arguments(rIntrospect, "data");
            }
         };

         INTERFACE(Properties)
         {
            Request<std::string, std::string> Get;
            Request<std::string, std::string, Variant<int>> Set;  // FIXME need type any instead!

            Response<Variant<int>> rGet;

            Properties()
             : INIT(Get)
             , INIT(Set)
             , INIT(rGet)
            {
               Get >> rGet;

               //describe_arguments(Get, "interface_name", "property_name");
               //describe_arguments(rGet, "value");
               //describe_arguments(Set, "interface_name", "property_name", "value");
            }
         };
      }
   }
}
#endif


namespace simppl
{

namespace dbus
{


/*static*/
DBusHandlerResult SkeletonBase::method_handler(DBusConnection* connection, DBusMessage* msg, void *user_data)
{
   SkeletonBase* skel = (SkeletonBase*)user_data;
   return skel->handleRequest(msg);
}


SkeletonBase::SkeletonBase(const char* iface, const char* role)
 : iface_(detail::extract_interface(iface))
 , busname_(nullptr)
 , objectpath_(nullptr)
 , disp_(nullptr)
{
   assert(role);

   objectpath_ = detail::create_objectpath(iface_, role);
   busname_ = detail::create_busname(iface_, role);
}


SkeletonBase::SkeletonBase(const char* iface, const char* busname, const char* objectpath)
 : iface_(detail::extract_interface(iface))
 , busname_(nullptr)
 , objectpath_(nullptr)
 , disp_(nullptr)
{
   assert(busname);
   assert(objectpath);

   busname_ = new char[strlen(busname) + 1];
   strcpy(busname_, busname);

   objectpath_ = new char[strlen(objectpath)+1];
   strcpy(objectpath_, objectpath);
}


SkeletonBase::~SkeletonBase()
{
   delete[] iface_;
   delete[] busname_;
   delete[] objectpath_;
}


Dispatcher& SkeletonBase::disp()
{
   assert(disp_);
   return *disp_;
}


ServerRequestDescriptor SkeletonBase::deferResponse()
{
   assert(current_request_);
  // assert(current_request_.requestor_->hasResponse());

   return std::move(current_request_);
}


void SkeletonBase::respondWith(detail::ServerResponseHolder response)
{
   assert(current_request_);
   //assert(response.responder_->allowedRequests_.find(current_request_.requestor_) != response.responder_->allowedRequests_.end());

   dbus_message_ptr_t rmsg = make_message(dbus_message_new_method_return(current_request_.msg_));

   if (response.f_)
   {
      detail::Serializer s(rmsg.get());
      response.f_(s);
   }

   dbus_connection_send(disp_->conn_, rmsg.get(), nullptr);

   current_request_.clear();   // only respond once!!!
}


void SkeletonBase::respondOn(ServerRequestDescriptor& req, detail::ServerResponseHolder response)
{
   assert(req);
   //assert(response.responder_->allowedRequests_.find(req.requestor_) != response.responder_->allowedRequests_.end());

   dbus_message_ptr_t rmsg = make_message(dbus_message_new_method_return(req.msg_));

   if (response.f_)
   {
      detail::Serializer s(rmsg.get());
      response.f_(s);
   }

   dbus_connection_send(disp_->conn_, rmsg.get(), nullptr);

   req.clear();
}


void SkeletonBase::respondWith(const Error& err)
{
   assert(current_request_);
   //assert(current_request_.requestor_->hasResponse());

   dbus_message_ptr_t rmsg = err.make_reply_for(*currentRequest().msg_);
   dbus_connection_send(disp_->conn_, rmsg.get(), nullptr);

   current_request_.clear();   // only respond once!!!
}


void SkeletonBase::respondOn(ServerRequestDescriptor& req, const Error& err)
{
   assert(req);
   //assert(req.requestor_->hasResponse());

   dbus_message_ptr_t rmsg = err.make_reply_for(*req.msg_);
   dbus_connection_send(disp_->conn_, rmsg.get(), nullptr);

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
#if SIMPPL_HAVE_INTROSPECTION
      if (!strcmp(method, "Introspect"))
      {
         std::ostringstream oss;

         oss << "<?xml version=\"1.0\" ?>\n"
             "<node name=\""<< objectpath() << "\">\n"
             "  <interface name=\""<< iface() << "\">\n";

         auto& methods = dynamic_cast<InterfaceBase<ServerRequest>*>(this)->methods_;
         for(auto& method : methods)
         {
            method.second->introspect(oss);
         }

         auto& attributes = dynamic_cast<InterfaceBase<ServerRequest>*>(this)->attributes_;
         for(auto& attribute : attributes)
         {
            attribute.second->introspect(oss);
         }

         auto& signals = dynamic_cast<InterfaceBase<ServerRequest>*>(this)->signals_;
         for(auto& sig : signals)
         {
            sig.second->introspect(oss);
         }

         // introspectable
         oss << "  </interface>\n"
             "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
             "    <method name=\"Introspect\">\n"
             "      <arg name=\"data\" type=\"s\" direction=\"out\"/>\n"
             "    </method>\n"
             "  </interface>\n";

         // attributes
         oss <<
             "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
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
             "  </interface>\n"
             "</node>\n";

         DBusMessage* reply = dbus_message_new_method_return(msg);

         detail::Serializer s(reply);
         s << oss.str();

         dbus_connection_send(disp_->conn_, reply, nullptr);

         return DBUS_HANDLER_RESULT_HANDLED;
      }
#endif   // #if SIMPPL_HAVE_INTROSPECTION
   }
   else if (!strcmp(interface, "org.freedesktop.DBus.Properties"))
   {
       if (!strcmp(method, "Get") || !strcmp(method, "Set"))
       {
          auto& attributes = dynamic_cast<InterfaceBase<ServerRequest>*>(this)->attributes_;

          std::string interface;
          std::string attribute;

          detail::Deserializer ds(msg);
          ds >> interface >> attribute;

          auto iter = attributes.find(attribute);
          if (iter != attributes.end())
          {
             if (method[0] == 'G')
             {
                dbus_message_ptr_t response = make_message(dbus_message_new_method_return(msg));
                iter->second->eval(response.get());

                dbus_connection_send(disp_->conn_, response.get(), nullptr);
             }
             else
             {
                iter->second->evalSet(ds);

                dbus_message_ptr_t response = make_message(dbus_message_new_method_return(msg));
                dbus_connection_send(disp_->conn_, response.get(), nullptr);
             }

             return DBUS_HANDLER_RESULT_HANDLED;
          }
          else
             std::cerr << "attribute '" << attribute << "' unknown" << std::endl;
       }
       else
          std::cerr << "unsupported Properties call method '" << method << "'" << std::endl;
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
             //assert(!current_request_.requestor_->hasResponse());
             current_request_.clear();
          }

          return DBUS_HANDLER_RESULT_HANDLED;
      }
      else
         std::cout << "method '" << method << "' unknown" << std::endl;
   }

   return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


}   // namespace dbus

}   // namespace simppl
