#ifndef SIMPPL_CLIENTSIDE_H
#define SIMPPL_CLIENTSIDE_H


#include <iostream>
#include <functional>

#include <dbus/dbus.h>

#include "simppl/calltraits.h"
#include "simppl/callstate.h"
#include "simppl/attribute.h"
#include "simppl/serialization.h"
#include "simppl/stubbase.h"
#include "simppl/timeout.h"

#include "simppl/detail/validation.h"
#include "simppl/detail/basicinterface.h"
#include "simppl/detail/clientresponseholder.h"


namespace simppl
{

namespace ipc
{

// forward decl
template<typename> struct InterfaceNamer;


// FIXME remove these types of interfaces in favour of std::function interface
struct ClientResponseBase
{
    static
    void pending_notify(DBusPendingCall* pending, void* user_data)
    {
std::cout << "Pending notify" << std::endl;
        ClientResponseBase* handler = (ClientResponseBase*)user_data;
        handler->eval(pending);
    }

   virtual void eval(DBusPendingCall* msg) = 0;

protected:

   inline
   ~ClientResponseBase()
   {
      // NOOP
   }
};


// ---------------------------------------------------------------------------------


struct ClientSignalBase
{
   virtual void eval(DBusMessage* msg) = 0;

   inline
   ClientSignalBase(const char* name, detail::BasicInterface* iface)
    : iface_(iface)
    , name_(name)
   {
      // NOOP
   }


   const char* name() const
   {
       return name_;
   }


// FIXME protected:


   inline
   ~ClientSignalBase()
   {
      // NOOP
   }

   detail::BasicInterface* iface_;
   const char* name_;
};


template<typename... T>
struct ClientSignal : ClientSignalBase
{
   static_assert(detail::isValidType<T...>::value, "invalid type in interface");

   typedef std::function<void(typename CallTraits<T>::param_type...)> function_type;

   inline
   ClientSignal(const char* name, detail::BasicInterface* iface)
    : ClientSignalBase(name, iface)
   {
      // NOOP
   }

   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      f_ = func;
   }

   /// send registration to the server - only attach after the interface is connected.
   inline
   ClientSignal& attach()
   {
      dynamic_cast<StubBase*>(iface_)->sendSignalRegistration(*this);
      return *this;
   }

   /// send de-registration to the server - only attach after the interface is connected.
   inline
   ClientSignal& detach()
   {
      dynamic_cast<StubBase*>(iface_)->sendSignalUnregistration(*this);
      return *this;
   }


   void eval(DBusMessage* msg)
   {
      if (f_)
      {
          detail::Deserializer d(msg);
          detail::GetCaller<T...>::type::template eval(d, f_);
      }
   }

   function_type f_;
};


// ---------------------------------------------------------------------------------------------


template<typename VectorT>
struct ClientVectorAttributeUpdate
{
   inline
   ClientVectorAttributeUpdate()
    : how_(None)
    , where_(0)
    , len_(0)
   {
      // NOOP
   }

   VectorT data_;
   How how_;
   uint32_t where_;
   uint32_t len_;
};


namespace detail
{

template<typename VectorT>
struct isValidType<ClientVectorAttributeUpdate<VectorT>>
{
   enum { value = true };
};

}   // namespace detail


// -----------------------------------------------------------------------------------


template<typename DataT, typename EmitPolicyT>
struct ClientAttribute
{
   static_assert(detail::isValidType<DataT>::value, "invalid type in interface");

   typedef typename CallTraits<DataT>::param_type arg_type;

   typedef typename if_<detail::is_vector<DataT>::value,
      std::function<void(arg_type, How, uint32_t, uint32_t)>,
      std::function<void(arg_type)> >::type function_type;

   inline
   ClientAttribute(const char* name, detail::BasicInterface* iface)
    : signal_(name, iface)
    , data_()
    , last_update_(0)
   {
      // NOOP
   }

   inline
   const DataT& value() const
   {
      return data_;
   }


   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      f_ = func;
   }

   /// only call this after the server is connected.
   inline
   ClientAttribute& attach()
   {
      signal_.handledBy(std::bind(&ClientAttribute<DataT, EmitPolicyT>::valueChanged, this, std::placeholders::_1));

      (void)signal_.attach();

      dynamic_cast<StubBase*>(signal_.iface_)->getProperty(signal_.name(), &pending_notify, this);

      return *this;
   }

   /// only call this after the server is connected.
   inline
   ClientAttribute& detach()
   {
      (void)signal_.detach();
      return *this;
   }

   inline
   time_t lastUpdate() const
   {
      return last_update_;
   }

private:

    static
    void pending_notify(DBusPendingCall* pending, void* user_data)
    {
        std::cout << "Pending attribute notify" << std::endl;
        ClientAttribute* handler = (ClientAttribute*)user_data;
        handler->eval(pending);
    }

    void eval(DBusPendingCall* pending)
    {
        DBusMessage* msg = dbus_pending_call_steal_reply(pending);
        if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
        {
            DBusMessageIter args;
            dbus_message_iter_init(msg, &args);

            int i;
            dbus_message_iter_get_basic(&args, &i);

            setAndCall(i);
        }
    }

   /// vector argument with partial update support
   typedef typename if_<detail::is_vector<DataT>::value, ClientVectorAttributeUpdate<DataT>, DataT>::type signal_arg_type;
   typedef ClientSignal<signal_arg_type> signal_type;

   void setAndCall(const ClientVectorAttributeUpdate<DataT>& varg)
   {
      switch (varg.how_)
      {
      case Full:
         data_ = varg.data_;
         break;

      case Remove:
         data_.erase(data_.begin()+varg.where_, data_.begin() + varg.where_ + varg.len_);
         break;

      case Insert:
         data_.insert(data_.begin()+varg.where_, varg.data_.begin(), varg.data_.end());
         break;

      case Replace:
         assert(varg.data_.size() == varg.len_);

         for(int i=0; i<varg.len_; ++i)
         {
            if (i+varg.where_ < data_.size())
            {
               data_[i+varg.where_] = varg.data_[i];
            }
            else
               data_.push_back(varg.data_[i]);
         }
         break;

      default:
         // NOOP
         break;
      }

      if (f_)
         f_(data_, varg.how_, varg.where_, varg.len_);
   }

   /// normal argument - will not work with vectors and partial update support
   void setAndCall(arg_type arg)
   {
      data_ = arg;

      if (f_)
         f_(data_);
   }

   void valueChanged(typename CallTraits<signal_arg_type>::param_type arg)
   {
      last_update_ = ::time(0);
      setAndCall(arg);
   }

   signal_type signal_;
   DataT data_;
   time_t last_update_;

   function_type f_;
};


// --------------------------------------------------------------------------------


template<typename... T>
struct ClientRequest
{
   static_assert(detail::isValidType<T...>::value, "invalid_type_in_interface");

   inline
   ClientRequest(const char* method_name, detail::BasicInterface* parent)
    : method_name_(method_name)
    , handler_(0)
    , parent_(parent)   // must take BasicInterface here and dynamic_cast later(!) since object hierarchy is not yet fully instantiated
   {
      // NOOP
   }

   inline
   detail::ClientResponseHolder operator()(typename CallTraits<T>::param_type... t)
   {
      StubBase* stub = dynamic_cast<StubBase*>(parent_);
      DBusMessage* msg = dbus_message_new_method_call(stub->destination(), stub->role(), stub->iface(), method_name_);
      DBusPendingCall* pending = nullptr;

      detail::Serializer s(msg);
      serialize(s, t...);
      
      if (handler_)
      {
         dbus_connection_send_with_reply(parent_->conn_, msg, &pending, -1/*FIXME detail::request_specific_timeout.count()*/);
         dbus_pending_call_set_notify(pending, &ClientResponseBase::pending_notify, handler_, 0);
      }
      else
         dbus_connection_send(parent_->conn_, msg, nullptr);

      dbus_message_unref(msg);

      return detail::ClientResponseHolder(handler_, pending);
   }

   inline
   ClientRequest& operator[](int flags)
   {
      assert(handler_);   // no oneway requests

      if (flags & (1<<0))
         detail::request_specific_timeout = timeout.timeout_;

      return *this;
   }

   // FIXME do we really need connection in BasicInterface?
   // FIXME do we need BasicInterface at all?
   ClientResponseBase* handler_;
   const char* method_name_;
   detail::BasicInterface* parent_;
};


// ----------------------------------------------------------------------------------------


template<typename... T>
struct ClientResponse : ClientResponseBase
{
   static_assert(detail::isValidType<T...>::value, "invalid_type_in_interface");

   typedef std::function<void(const CallState&, typename CallTraits<T>::param_type...)> function_type;

   inline
   ClientResponse(void*)
   {
       // NOOP
   }

   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      f_ = func;
   }

   void eval(DBusPendingCall* pc)
   {
      if (f_)
      {
          DBusMessage* msg = dbus_pending_call_steal_reply(pc);
          
          // FIXME error handling
          if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_ERROR)
          {
              DBusError err;
              dbus_error_init(&err);

              dbus_set_error_from_message(&err, msg);
                            
              if (!strcmp(err.name, DBUS_ERROR_FAILED))
              {
                  int error;
                  char buf[512];
                  sscanf(err.message, "%d %s", &error, buf);

                  f_(CallState(new RuntimeError(error, buf, dbus_message_get_reply_serial(msg))), 0);  /* FIXME dummy args */
              }
              else
              {
                  std::cout << "Having transport error: name=" << err.name << ", message=" << err.message << std::endl;
                  f_(CallState(new TransportError(EIO, dbus_message_get_reply_serial(msg))), 0);  /* FIXME dummy args */
              }
              
              dbus_error_free(&err);
          }
          else
          {
             CallState cs(dbus_message_get_reply_serial(msg));
             
             detail::Deserializer d(msg);
             detail::GetCaller<T...>::type::template evalResponse(d, f_, cs);
          }
          
          dbus_message_unref(msg);
      }
      else
         std::cerr << "No response handler installed" << std::endl;
   }

   function_type f_;
};


}   // namespace ipc

}   // namespace simppl


template<typename VectorT>
simppl::ipc::detail::Deserializer& operator>>(simppl::ipc::detail::Deserializer& istream, simppl::ipc::ClientVectorAttributeUpdate<VectorT>& updt)
{
   istream >> (uint32_t&)updt.how_;
   istream >> updt.where_;
   istream >> updt.len_;

   if (updt.how_ != simppl::ipc::Remove)
   {
      updt.data_.resize(updt.len_);

      for(int i=0; i<updt.len_; ++i)
      {
         istream >> updt.data_[i];
      }
   }

   return istream;
}


template<typename FunctorT, typename... T>
inline
simppl::ipc::ClientResponse<T...>& operator>>(simppl::ipc::ClientResponse<T...>& r, const FunctorT& f)
{
   r.handledBy(f);
   return r;
}


template<typename DataT, typename EmitPolicyT, typename FuncT>
inline
void operator>>(simppl::ipc::ClientAttribute<DataT,EmitPolicyT>& attr, const FuncT& func)
{
   attr.handledBy(func);
}


template<typename... T, typename FuncT>
inline
void operator>>(simppl::ipc::ClientSignal<T...>& sig, const FuncT& func)
{
   sig.handledBy(func);
}


#include "simppl/stub.h"

#endif   // SIMPPL_CLIENTSIDE_H
