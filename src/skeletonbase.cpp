#include "simppl/skeletonbase.h"

#include "simppl/dispatcher.h"
#include "simppl/interface.h"

#define SIMPPL_SKELETONBASE_CPP
#include "simppl/serverside.h"
#undef SIMPPL_SKELETONBASE_CPP

#include "simppl/detail/util.h"

#include <iostream>


namespace simppl
{

namespace dbus
{


/*static*/
DBusHandlerResult SkeletonBase::method_handler(DBusConnection* connection, DBusMessage* msg, void *user_data)
{
   SkeletonBase* skel = (SkeletonBase*)user_data;
   return skel->handle_request(msg);
}


SkeletonBase::SkeletonBase()
 : iface_(nullptr)
 , busname_(nullptr)
 , objectpath_(nullptr)
 , disp_(nullptr)
 , methods_(nullptr)
 , properties_(nullptr)
#if SIMPPL_HAVE_INTROSPECTION
 , signals_(nullptr)
#endif
{
   // NOOP
}


SkeletonBase::~SkeletonBase()
{
   delete[] iface_;
   delete[] busname_;
   delete[] objectpath_;
}


void SkeletonBase::init(char* iface, const char* role)
{
    assert(role);

    iface_ = detail::extract_interface(iface);

    objectpath_ = detail::create_objectpath(iface_, role);
    busname_ = detail::create_busname(iface_, role);

    free(iface);
}


void SkeletonBase::init(char* iface, const char* busname, const char* objectpath)
{
    assert(busname);
    assert(objectpath);

    iface_ = detail::extract_interface(iface);

    busname_ = new char[strlen(busname) + 1];
    strcpy(busname_, busname);

    objectpath_ = new char[strlen(objectpath)+1];
    strcpy(objectpath_, objectpath);

    free(iface);
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
      detail::Serializer s(rmsg.get());
      response.f_(s);
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
      detail::Serializer s(rmsg.get());
      response.f_(s);
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

         auto pm = this->methods_;
         while(pm)
         {
            pm->introspect(oss);
            pm = pm->next_;
         }

         auto pa = this->properties_;
         while(pa)
         {
            pa->introspect(oss);
            pa = pa->next_;
         }

         auto ps = this->signals_;
         while(ps)
         {
            ps->introspect(oss);
            ps = ps->next_;
         }

         // introspectable
         oss << "  </interface>\n"
             "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
             "    <method name=\"Introspect\">\n"
             "      <arg name=\"data\" type=\"s\" direction=\"out\"/>\n"
             "    </method>\n"
             "  </interface>\n";

         // attributes
         if (this->properties_)
         {
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
               "    <signal name=\"PropertiesChanged\">\n"
               "      <arg name=\"interface_name\" type=\"s\"/>\n"
               "      <arg name=\"changed_properties\" type=\"a{sv}\"/>\n"
               "      <arg name=\"invalidated_properties\" type=\"as\"/>\n"
               "    </signal>\n"
               "  </interface>\n";
         }

         oss << "</node>\n";

         DBusMessage* reply = dbus_message_new_method_return(msg);

         detail::Serializer s(reply);
         s.write(oss.str());

         dbus_connection_send(disp_->conn_, reply, nullptr);

         return DBUS_HANDLER_RESULT_HANDLED;
      }
#endif   // #if SIMPPL_HAVE_INTROSPECTION
   }
   else if (!strcmp(interface, "org.freedesktop.DBus.Properties"))
   {
       if (!strcmp(method, "Get") || !strcmp(method, "Set"))
       {
          auto p = this->properties_;

          std::string interface;
          std::string attribute;

          detail::Deserializer ds(msg);
          ds.read(interface).read(attribute);

          while(p)
          {
             if (attribute == p->name_)
             {
                if (method[0] == 'G')
                {
                   message_ptr_t response = make_message(dbus_message_new_method_return(msg));
                   p->eval(response.get());

                   dbus_connection_send(disp_->conn_, response.get(), nullptr);
                }
                else
                {
                   message_ptr_t response = make_message(nullptr);
                   
                   try
                   {
                      p->evalSet(ds);
                     
                      response = make_message(dbus_message_new_method_return(msg));
                   }
                   catch(simppl::dbus::Error& err)
                   {
                      response = err.make_reply_for(*msg);
                   }
                   catch(...)
                   {
                      simppl::dbus::Error e("simppl.dbus.UnhandledException");
                      response = e.make_reply_for(*msg);
                   }
                   
                   dbus_connection_send(disp_->conn_, response.get(), nullptr);
                }

                return DBUS_HANDLER_RESULT_HANDLED;
             }

             p = p->next_;
          }

          std::cerr << "attribute '" << attribute << "' unknown" << std::endl;
       }
   }
   else
   {
      auto pm = this->methods_;
      while(pm)
      {
         if (!strcmp(method, pm->name_))
         {
#if SIMPPL_SIGNATURE_CHECK
            if (strcmp(pm->get_signature(), dbus_message_get_signature(msg)))
            {
               std::cerr << "Shit, wrong arguments" << std::endl;
               return DBUS_HANDLER_RESULT_HANDLED;
            }
#endif

            current_request_.set(pm, msg);
            pm->eval(msg);

            // current_request_ is only valid if no response handler was called
            if (current_request_)
            {
               // in that case the request must not have a reponse
               //assert(!current_request_.requestor_->hasResponse());
               current_request_.clear();
            }

            return DBUS_HANDLER_RESULT_HANDLED;
         }

         pm = pm->next_;
      }

      std::cerr << "method '" << method << "' unknown" << std::endl;
   }

   return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


void SkeletonBase::send_signal(const char* signame, std::function<void(detail::Serializer&)>&& f)
{
    message_ptr_t msg = make_message(dbus_message_new_signal(objectpath(), iface(), signame));

    detail::Serializer s(msg.get());
    f(s);

    dbus_connection_send(disp_->conn_, msg.get(), nullptr);
}


void SkeletonBase::send_property_change(const char* prop, std::function<void(detail::Serializer&)>&& f)
{
   static std::vector<std::string> invalid;

   message_ptr_t msg = make_message(dbus_message_new_signal(objectpath(), "org.freedesktop.DBus.Properties", "PropertiesChanged"));

   detail::Serializer s(msg.get());
   s.write(iface());

   // TODO make once
   std::ostringstream buf;
   buf << DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
       << DBUS_TYPE_STRING_AS_STRING
       << DBUS_TYPE_VARIANT_AS_STRING
       << DBUS_DICT_ENTRY_END_CHAR_AS_STRING;

   DBusMessageIter iter;
   dbus_message_iter_open_container(s.iter_, DBUS_TYPE_ARRAY, buf.str().c_str(), &iter);

   DBusMessageIter item_iterator;
   dbus_message_iter_open_container(&iter, DBUS_TYPE_DICT_ENTRY, nullptr, &item_iterator);

   detail::Serializer des(&item_iterator);
   des.write(prop);
   f(des);

   // the dict entry
   dbus_message_iter_close_container(&iter, &item_iterator);

   // the map
   dbus_message_iter_close_container(s.iter_, &iter);

   s.write(invalid);

   dbus_connection_send(disp_->conn_, msg.get(), nullptr);
}


}   // namespace dbus

}   // namespace simppl
