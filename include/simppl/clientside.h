#ifndef SIMPPL_CLIENTSIDE_H
#define SIMPPL_CLIENTSIDE_H


#include <iostream>
#include <functional>

#include <dbus/dbus.h>

#include "simppl/calltraits.h"
#include "simppl/callstate.h"
#include "simppl/property.h"
#include "simppl/serialization.h"
#include "simppl/stubbase.h"
#include "simppl/timeout.h"
#include "simppl/parameter_deduction.h"

#include "simppl/detail/validation.h"
#include "simppl/detail/callinterface.h"
#include "simppl/detail/holders.h"
#include "simppl/detail/basicinterface.h"
#include "simppl/detail/deserialize_and_return.h"


namespace simppl
{

namespace dbus
{

// forward decl
template<typename> struct InterfaceNamer;


// ---------------------------------------------------------------------------------


struct ClientSignalBase
{
   template<typename, int> 
   friend struct ClientProperty;
   
   
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


protected:

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
   void handled_by(FunctorT func)
   {
      f_ = func;
   }

   /// send registration to the server - only attach after the interface is connected.
   inline
   ClientSignal& attach()
   {
      dynamic_cast<StubBase*>(iface_)->register_signal(*this);
      return *this;
   }

   /// send de-registration to the server - only attach after the interface is connected.
   inline
   ClientSignal& detach()
   {
      dynamic_cast<StubBase*>(iface_)->unregister_signal(*this);
      return *this;
   }


private:

   void eval(DBusMessage* msg)
   {
      detail::Deserializer d(msg);
      detail::GetCaller<std::tuple<T...>>::type::template eval(d, f_);
   }

   function_type f_;
};


// ---------------------------------------------------------------------------------------------


template<typename PropertyT, typename DataT>
struct ClientPropertyWritableMixin
{
   typedef DataT data_type;
   typedef typename CallTraits<DataT>::param_type arg_type;
   typedef std::function<void(CallState)> function_type;
   typedef detail::CallbackHolder<function_type, void> holder_type;


   /// blocking version
   void set(arg_type t)
   {
      auto that = (PropertyT*)this;
   
      Variant<data_type> vt(t);

      that->stub().set_property(that->name(), [&vt](detail::Serializer& s){
         simppl::dbus::detail::serialize(s, vt);
      });
   }


   /// async version
   detail::InterimCallbackHolder<holder_type> set_async(arg_type t)
   {
      auto that = (PropertyT*)this;

      Variant<data_type> vt(t);

      return detail::InterimCallbackHolder<holder_type>(that->stub().set_property_async(that->name(), [&vt](detail::Serializer& s){
         simppl::dbus::detail::serialize(s, vt);
      }));
   }
};


struct NoopMixin
{
};


template<typename DataT, int Flags = Notifying|ReadOnly>
struct ClientProperty
 : std::conditional<(Flags & ReadWrite), ClientPropertyWritableMixin<ClientProperty<DataT, Flags>, DataT>, NoopMixin>::type
{
   static_assert(detail::isValidType<DataT>::value, "invalid attribute type");

   typedef DataT data_type;
   typedef typename CallTraits<DataT>::param_type arg_type;
   typedef std::function<void(CallState, arg_type)> function_type;
   typedef ClientSignal<DataT> signal_type;
   typedef detail::PropertyCallbackHolder<std::function<void(CallState, arg_type)>, data_type> holder_type;  


   inline
   ClientProperty(const char* name, detail::BasicInterface* iface)
    : name_(name)
    , iface_(iface)
   {
      // NOOP
   }
   

   template<typename FunctorT>
   inline
   void handled_by(FunctorT func)
   {
      f_ = func;
   }

   StubBase& stub()
   {
      return *dynamic_cast<StubBase*>(iface_);
   }

   /// only call this after the server is connected.
   ClientProperty& attach()
   {
      stub().attach_property(name_, [this](detail::Deserializer& s){
         
         Variant<data_type> d;
         s >> d;
         
         if (this->f_)
            this->f_(CallState(42), *d.template get<data_type>());
      });
      
      dbus_pending_call_set_notify(stub().get_property_async(name_), 
         &holder_type::pending_notify, 
         new holder_type([this](CallState cs, const arg_type& val){
            if (f_)
               f_(cs, val);
         }), 
         &holder_type::_delete);
      
      return *this;
   }


   // TODO implement GetAll 
   DataT get()
   {
      message_ptr_t msg = stub().get_property(name_);

      detail::Deserializer ds(msg.get());

      Variant<DataT> v;
      ds >> v;

      return *v.template get<DataT>();
   }


   inline
   detail::InterimCallbackHolder<holder_type> get_async()
   {
      return detail::InterimCallbackHolder<holder_type>(stub().get_property_async(name_));
   }


   /// blocking version
   ClientProperty& operator=(arg_type t)
   {
      this->set(t);
      return *this;
   }

   /// only call this after the server is connected.
   inline
   ClientProperty& detach()
   {
      stub().detach_property(name_);
      return *this;
   }


   inline
   const char* name() const
   {
      return name_;
   }

private:

   const char* name_;
   detail::BasicInterface* iface_;
   
   function_type f_;
};


// --------------------------------------------------------------------------------


template<typename... ArgsT>
struct ClientRequest
{
    typedef detail::generate_argument_type<ArgsT...>  args_type_generator;
    typedef detail::generate_return_type<ArgsT...>    return_type_generator;

    enum {
        valid     = AllOf<typename make_typelist<ArgsT...>::type, detail::InOutOrOneway>::value,
        valid_in  = AllOf<typename args_type_generator::list_type, detail::IsValidTypeFunctor>::value,
        valid_out = AllOf<typename return_type_generator::list_type, detail::IsValidTypeFunctor>::value,
        is_oneway = detail::is_oneway_request<ArgsT...>::value
    };

    typedef typename detail::canonify<typename args_type_generator::const_type>::type    args_type;
    typedef typename detail::canonify<typename return_type_generator::type>::type        return_type;

    typedef typename detail::generate_callback_function<ArgsT...>::type                  callback_type;
    typedef detail::CallbackHolder<callback_type, return_type>                           holder_type;
     
    // correct typesafe serializer
    typedef typename detail::make_serializer_from_list<
        typename args_type_generator::const_list_type, detail::SerializerGenerator<>>::type serializer_type;
    
    static_assert(!is_oneway || (is_oneway && std::is_same<return_type, void>::value), "oneway check");


    inline
    ClientRequest(const char* method_name, detail::BasicInterface* parent)
     : method_name_(method_name)
     , parent_(parent)   // must take BasicInterface here and dynamic_cast later(!) since object hierarchy is not yet fully instantiated
    {
      // NOOP
    }


   /// blocking call
   template<typename... T>
   return_type operator()(const T&... t)
   {
//      std::cout << abi::__cxa_demangle(typeid(typename detail::canonify<std::tuple<T...>>::type).name(), 0, 0, 0) << std::endl;
//      std::cout << abi::__cxa_demangle(typeid(args_type).name(), 0, 0, 0) << std::endl;
      
      static_assert(std::is_convertible<typename detail::canonify<std::tuple<T...>>::type,
                    args_type>::value, "args mismatch");

      auto stub = dynamic_cast<StubBase*>(parent_);

      auto p = make_pending_call(stub->send_request(method_name_, [&](detail::Serializer& s){
         serializer_type::eval(s, t...);
      }, is_oneway));

      // TODO move this stuff into the stub baseclass, including blocking on pending call,
      // stealing reply, eval callstate, throw exception, ...
      if (!is_oneway)
      {
         dbus_pending_call_block(p.get());

         message_ptr_t msg = make_message(dbus_pending_call_steal_reply(p.get()));
         CallState cs(*msg);

         if (!cs)
            cs.throw_exception();

         return detail::deserialize_and_return<return_type>::eval(msg.get());
      }
      else
      {
         dbus_connection_flush(stub->conn());
         return return_type();
      }
   }


   /// asynchronous call
   template<typename... T>
   detail::InterimCallbackHolder<holder_type> async(const T&... t)
   {
      static_assert(is_oneway == false, "it's a oneway function");
      static_assert(std::is_convertible<typename detail::canonify<std::tuple<typename std::decay<T>::type...>>::type,
                    args_type>::value, "args mismatch");

      auto stub = dynamic_cast<StubBase*>(parent_);

      return detail::InterimCallbackHolder<holder_type>(stub->send_request(method_name_, [&](detail::Serializer& s){
         serializer_type::eval(s, t...);
      }, false));
   }


   ClientRequest& operator[](int flags)
   {
      static_assert(is_oneway == false, "it's a oneway function");

      if (flags & (1<<0))
         detail::request_specific_timeout = timeout.timeout_;

      return *this;
   }


private:

   const char* method_name_;
   detail::BasicInterface* parent_;
};


// ----------------------------------------------------------------------------------------


}   // namespace dbus

}   // namespace simppl


template<typename HolderT, typename FunctorT>
inline
void operator>>(simppl::dbus::detail::InterimCallbackHolder<HolderT>&& r, const FunctorT& f)
{
   // TODO static_assert FunctorT and HolderT::f_ convertible?
   dbus_pending_call_set_notify(r.pc_.release(), &HolderT::pending_notify, new HolderT(f), &HolderT::_delete);
}


template<typename DataT, int Flags, typename FuncT>
inline
void operator>>(simppl::dbus::ClientProperty<DataT, Flags>& attr, const FuncT& func)
{
   attr.handled_by(func);
}


template<typename... T, typename FuncT>
inline
void operator>>(simppl::dbus::ClientSignal<T...>& sig, const FuncT& func)
{
   sig.handled_by(func);
}


#include "simppl/stub.h"

#endif   // SIMPPL_CLIENTSIDE_H
