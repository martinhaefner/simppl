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
#include "simppl/parameter_deduction.h"

#include "simppl/detail/validation.h"
#include "simppl/detail/basicinterface.h"
#include "simppl/detail/clientresponseholder.h"
#include "simppl/detail/deserialize_and_return.h"
#include "simppl/detail/callinterface.h"


namespace simppl
{

namespace dbus
{

// forward decl
template<typename> struct InterfaceNamer;


// FIXME remove these types of interfaces in favour of std::function interface
struct ClientResponseBase
{
   virtual void eval(DBusMessage& msg) = 0;

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

   inline
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
      detail::Deserializer d(msg);
      detail::GetCaller<T...>::type::template eval(d, f_);
   }

   function_type f_;
};


// ---------------------------------------------------------------------------------------------


template<typename AttributeT, typename DataT>
struct ClientAttributeWritableMixin
{
   typedef DataT data_type;
   typedef typename CallTraits<DataT>::param_type arg_type;


   void set(arg_type t)
   {
      AttributeT* that = (AttributeT*)this;
      that->data_ = t;

      Variant<data_type> vt(t);

      std::function<void(detail::Serializer&)> f = [&vt](detail::Serializer& s){
         simppl::dbus::detail::serialize(s, vt);
      };

      that->stub().setProperty(that->signal_.name(), f);
   }
};


struct NoopMixin
{
};


template<typename DataT, int Flags = Notifying|ReadOnly>
struct ClientAttribute
 : std::conditional<(Flags & ReadWrite), ClientAttributeWritableMixin<ClientAttribute<DataT, Flags>, DataT>, NoopMixin>::type
{
   static_assert(detail::isValidType<DataT>::value, "invalid attribute type");

   typedef DataT data_type;
   typedef typename CallTraits<DataT>::param_type arg_type;
   typedef std::function<void(CallState, arg_type)> function_type;
   typedef ClientSignal<DataT> signal_type;


   inline
   ClientAttribute(const char* name, detail::BasicInterface* iface)
    : signal_(name, iface)
   {
      // NOOP
   }

   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      f_ = func;
   }

   StubBase& stub()
   {
      return *dynamic_cast<StubBase*>(signal_.iface_);
   }

   /// only call this after the server is connected.
   ClientAttribute& attach()
   {
      signal_.handledBy(std::bind(&ClientAttribute<DataT, Flags>::valueChanged, this, std::placeholders::_1));

      (void)signal_.attach();

      stub().getProperty(signal_.name(), &pending_notify, this);

      return *this;
   }

   inline
   const DataT& value() const
   {
      return data_;
   }

   // FIXME implement GetAll in Stub needs to store attributesbase in interface
   // in a similar way as it is done on server side...
   const DataT& get()
   {
      std::unique_ptr<DBusMessage, void(*)(DBusMessage*)> msg(stub().getProperty(signal_.name()), dbus_message_unref);

      detail::Deserializer ds(msg.get());

      Variant<DataT> v;
      ds >> v;

      data_ = *v.template get<DataT>();
      return data_;
   }


   ClientAttribute& get_async()
   {
      stub().getProperty(signal_.name(), &pending_notify, this);
      return *this;
   }


   ClientAttribute& operator=(arg_type t)
   {
      this->set(t);
      return *this;
   }

   /// only call this after the server is connected.
   inline
   ClientAttribute& detach()
   {
      (void)signal_.detach();
      return *this;
   }

//FIXME protected:

    static
    void pending_notify(DBusPendingCall* pc, void* user_data)
    {
        std::unique_ptr<DBusPendingCall, void(*)(DBusPendingCall*)> upc(pc, dbus_pending_call_unref);
        std::unique_ptr<DBusMessage, void(*)(DBusMessage*)> msg(dbus_pending_call_steal_reply(pc), dbus_message_unref);

        CallState cs(*msg);

        // TODO callstate should be part of the callback
        if (cs)
        {
           ClientAttribute* handler = (ClientAttribute*)user_data;
           handler->eval(*msg);
        }
    }

    void eval(DBusMessage& msg)
    {
        detail::Deserializer ds(&msg);

        // FIXME check type of variant and set error flag on stream?!
        Variant<DataT> v;
        ds >> v;

        data_ = *v.template get<DataT>();

        if (f_)
            f_(CallState(msg), *v.template get<DataT>());
    }


   void valueChanged(arg_type arg)
   {
      data_ = arg;

      if (f_)
         f_(CallState(42), arg);
   }

   signal_type signal_;
   function_type f_;
   DataT data_;
};


// --------------------------------------------------------------------------------


// FIXME could possibliy be dropped?!
struct ClientRequestBase
{
   ClientRequestBase(const char* method_name)
    : method_name_(method_name)
   {
       // NOOP
   }

   const char* method_name_;
};


template<typename... ArgsT>
struct ClientRequest : ClientRequestBase
{
    typedef detail::generate_argument_type<ArgsT...>  args_type_generator;
    typedef detail::generate_return_type<ArgsT...>    return_type_generator;

    enum {
        valid     = AllOf<typename make_typelist<ArgsT...>::type, detail::InOutOrOneway>::value,
        valid_in  = AllOf<typename args_type_generator::list_type, detail::IsValidTypeFunctor>::value,
        valid_out = AllOf<typename return_type_generator::list_type, detail::IsValidTypeFunctor>::value,
        is_oneway = detail::is_oneway_request<ArgsT...>::value
    };

    typedef typename detail::canonify<typename args_type_generator::type>::type    args_type;
    typedef typename detail::canonify<typename return_type_generator::type>::type  return_type;

    typedef typename detail::generate_callback_function<ArgsT...>::type            callback_type;

    static_assert(!is_oneway || is_oneway && std::is_same<return_type, void>::value, "oneway check");


    inline
    ClientRequest(const char* method_name, detail::BasicInterface* parent)
    : ClientRequestBase(method_name)
    , parent_(parent)   // must take BasicInterface here and dynamic_cast later(!) since object hierarchy is not yet fully instantiated
    {
      // NOOP
    }


   // blocking call
   template<typename... T>
   return_type operator()(const T&... t)
   {
      static_assert(std::is_convertible<typename detail::canonify<std::tuple<typename std::decay<T>::type...>>::type,
                    args_type>::value, "args mismatch");

      StubBase* stub = dynamic_cast<StubBase*>(parent_);

      std::function<void(detail::Serializer&)> f = [&](detail::Serializer& s){
         simppl::dbus::detail::serialize(s, t...);
      };

      std::unique_ptr<DBusPendingCall, void(*)(DBusPendingCall*)> p(stub->sendRequest(*this, f, is_oneway), dbus_pending_call_unref);

      // FIXME move this stuff into the stub baseclass, including blocking on pending call,
      // stealing reply, eval callstate, throw exception, ...
      if (!is_oneway)
      {
         dbus_pending_call_block(p.get());

         std::unique_ptr<DBusMessage, void(*)(DBusMessage*)> msg(dbus_pending_call_steal_reply(p.get()), dbus_message_unref);
         CallState cs(*msg);

         if (!cs)
            cs.throw_exception();

         return detail::deserialize_and_return<return_type>::eval(msg.get());
      }
      else
         dbus_connection_flush(stub->conn());
   }


   // async call
   template<typename... T>
   void async(const T&... t)
   {
      static_assert(is_oneway == false, "it's a oneway function");
      static_assert(std::is_convertible<typename detail::canonify<std::tuple<typename std::decay<T>::type...>>::type,
                    args_type>::value, "args mismatch");

      StubBase* stub = dynamic_cast<StubBase*>(parent_);

      std::function<void(detail::Serializer&)> f = [&](detail::Serializer& s){
         simppl::dbus::detail::serialize(s, t...);
      };

      dbus_pending_call_set_notify(stub->sendRequest(*this, f, false), &ClientRequest::pending_notify, this, 0);
   }


   static
   void pending_notify(DBusPendingCall* pc, void* data)
   {
       std::unique_ptr<DBusPendingCall, void(*)(DBusPendingCall*)> upc(pc, dbus_pending_call_unref);
       std::unique_ptr<DBusMessage, void(*)(DBusMessage*)> msg(dbus_pending_call_steal_reply(pc), dbus_message_unref);
       
       ClientRequest* that = (ClientRequest*)data;
       
       if (that->f_)
       {
           CallState cs(*msg);

           detail::Deserializer d(msg.get());
           detail::GetCaller<return_type>::type::template evalResponse(d, that->f_, cs);
       }
   }


   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      f_ = func;
   }


   ClientRequest& operator[](int flags)
   {
      static_assert(is_oneway == false, "it's a oneway function");

      if (flags & (1<<0))
         detail::request_specific_timeout = timeout.timeout_;

      return *this;
   }

   // FIXME do we really need connection in BasicInterface?
   // FIXME do we need BasicInterface at all?
   detail::BasicInterface* parent_;

   // nonblocking asynchronous call needs a callback function
   callback_type f_;

};


// ----------------------------------------------------------------------------------------


}   // namespace dbus

}   // namespace simppl


template<typename FunctorT, typename... T>
inline
simppl::dbus::ClientRequest<T...>& operator>>(simppl::dbus::ClientRequest<T...>& r, const FunctorT& f)
{
   r.handledBy(f);
   return r;
}


template<typename DataT, int Flags, typename FuncT>
inline
void operator>>(simppl::dbus::ClientAttribute<DataT, Flags>& attr, const FuncT& func)
{
   attr.handledBy(func);
}


template<typename... T, typename FuncT>
inline
void operator>>(simppl::dbus::ClientSignal<T...>& sig, const FuncT& func)
{
   sig.handledBy(func);
}


#include "simppl/stub.h"

#endif   // SIMPPL_CLIENTSIDE_H
