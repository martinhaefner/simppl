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


template<typename DataT, typename EmitPolicyT>
struct ClientAttribute
{
   static_assert(detail::isValidType<DataT>::value, "invalid type in interface");

   typedef typename CallTraits<DataT>::param_type arg_type;
   typedef std::function<void(arg_type)> function_type;
   typedef ClientSignal<DataT> signal_type;


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
//        std::cout << "Pending attribute notify" << std::endl;
        ClientAttribute* handler = (ClientAttribute*)user_data;
        handler->eval(pending);
    }

    void eval(DBusPendingCall* pending)
    {
        DBusMessage* msg = dbus_pending_call_steal_reply(pending);
        
        if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
        {
            last_update_ = ::time(0);
            
            detail::Deserializer ds(msg);
            ds >> data_;

            if (f_)
               f_(data_);
        }
    }


   void valueChanged(arg_type arg)
   {
      last_update_ = ::time(0);
      
      data_ = arg;
      if (f_)
         f_(data_);
   }

   signal_type signal_;
   DataT data_;
   time_t last_update_;

   function_type f_;
};


// --------------------------------------------------------------------------------


struct ClientRequestBase
{
   ClientRequestBase(const char* method_name)
    : method_name_(method_name)
    , handler_(0)
   {
       // NOOP
   }

   ClientResponseBase* handler_;
   const char* method_name_;
};


template<typename... T>
struct ClientRequest : ClientRequestBase
{
   static_assert(detail::isValidType<T...>::value, "invalid_type_in_interface");

   inline
   ClientRequest(const char* method_name, detail::BasicInterface* parent)
    : ClientRequestBase(method_name)
    , parent_(parent)   // must take BasicInterface here and dynamic_cast later(!) since object hierarchy is not yet fully instantiated
   {
      // NOOP
   }

   inline
   detail::ClientResponseHolder operator()(typename CallTraits<T>::param_type... t)
   {
      StubBase* stub = dynamic_cast<StubBase*>(parent_);

      std::function<void(detail::Serializer&)> f(std::bind(&simppl::ipc::detail::serializeN<typename CallTraits<T>::param_type...>, std::placeholders::_1, t...));
      return detail::ClientResponseHolder(stub->disp(), handler_, stub->sendRequest(*this, f));
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

          if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_ERROR)
          {
              if (!strcmp(dbus_message_get_error_name(msg), DBUS_ERROR_FAILED))
              {
                  detail::Deserializer d(msg);
                  std::string text;
                  d >> text;
                  
                  char* end;
                  int error = strtol(text.c_str(), &end, 10);
                  
                  std::tuple<T...> dummies;
                  detail::FunctionCaller<0, std::tuple<T...>>::template eval_cs(f_, CallState(new RuntimeError(error, end+1, dbus_message_get_reply_serial(msg))), dummies);
              }
              else
              {
                  std::tuple<T...> dummies;
                  detail::FunctionCaller<0, std::tuple<T...>>::template eval_cs(f_, CallState(new TransportError(EIO, dbus_message_get_reply_serial(msg))), dummies);
              }
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
