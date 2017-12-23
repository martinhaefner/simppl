#ifndef SIMPPL_CLIENTSIDE_H
#define SIMPPL_CLIENTSIDE_H


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
   typedef void (*eval_type)(ClientSignalBase*, DBusMessage*);
   
   template<typename, int>
   friend struct ClientProperty;


   void eval(DBusMessage* msg)
   {
      eval_(this, msg);
   }
   
   //virtual const char* get_signature() const = 0;

   inline
   ClientSignalBase(const char* name, StubBase* iface)
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
   ~ClientSignalBase() = default;

   StubBase* iface_;
   const char* name_;
   
   eval_type eval_;
};


template<typename... T>
struct ClientSignal : ClientSignalBase
{
   static_assert(detail::isValidType<T...>::value, "invalid type in interface");

   typedef std::function<void(typename CallTraits<T>::param_type...)> function_type;

   inline
   ClientSignal(const char* name, StubBase* iface)
    : ClientSignalBase(name, iface)
   {
      eval_ = __eval;
   }

   /// send registration to the server - only attach after the interface is connected.
   inline
   ClientSignal& attach()
   {
      iface_->register_signal(*this);
      return *this;
   }

   /// send de-registration to the server - only attach after the interface is connected.
   inline
   ClientSignal& detach()
   {
      iface_->unregister_signal(*this);
      return *this;
   }
   
   
   // TODO make >> a friend
   function_type f_;


private:

   static 
   void __eval(ClientSignalBase* obj, DBusMessage* msg)
   {
      ClientSignal* that = (ClientSignal*)(obj);
      
      DBusMessageIter iter;
      dbus_message_iter_init(msg, &iter);
      
      detail::GetCaller<std::tuple<T...>>::type::template eval(iter, that->f_);
   }
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

      that->stub().set_property(that->name(), [&vt](DBusMessageIter& s){
         simppl::dbus::detail::serialize(s, vt);
      });
   }


   /// async version
   detail::InterimCallbackHolder<holder_type> set_async(arg_type t)
   {
      auto that = (PropertyT*)this;

      Variant<data_type> vt(t);

      return detail::InterimCallbackHolder<holder_type>(that->stub().set_property_async(that->name(), [&vt](DBusMessageIter& s){
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
   ClientProperty(const char* name, StubBase* iface)
    : name_(name)
    , iface_(iface)
   {
      // NOOP
   }

   StubBase& stub()
   {
      return *iface_;
   }

   /// only call this after the server is connected.
   ClientProperty& attach();

   // TODO implement GetAll
   DataT get()
   {
      message_ptr_t msg = stub().get_property(name_);

      DBusMessageIter iter;
      dbus_message_iter_init(msg.get(), &iter);

      Variant<DataT> v;
      Codec<Variant<DataT>>::decode(iter, v);

      return std::move(*v.template get<DataT>());
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

   
   function_type f_;

private:

   const char* name_;
   StubBase* iface_;
};


/// only call this after the server is connected.
template<typename DataT, int Flags>
ClientProperty<DataT, Flags>& ClientProperty<DataT, Flags>::attach()
{
  stub().attach_property(name_, [this](DBusMessageIter& iter){

     Variant<data_type> d;
     Codec<Variant<data_type>>::decode(iter, d);

     if (this->f_)
        this->f_(CallState(42), *d.template get<data_type>());
  });

  dbus_pending_call_set_notify(stub().get_property_async(name_).pending(),
     &holder_type::pending_notify,
     new holder_type([this](CallState cs, const arg_type& val){
        if (f_)
           f_(cs, val);
     }),
     &holder_type::_delete);

  return *this;
}


// --------------------------------------------------------------------------------


template<typename... ArgsT>
struct ClientMethod
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
    typedef typename detail::generate_serializer<typename args_type_generator::const_list_type>::type serializer_type;

    static_assert(!is_oneway || (is_oneway && std::is_same<return_type, void>::value), "oneway check");


    inline
    ClientMethod(const char* method_name, StubBase* parent)
     : method_name_(method_name)
     , parent_(parent)   
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

      auto msg = parent_->send_request_and_block(method_name_, [&](DBusMessageIter& s){
         serializer_type::eval(s, t...);
      }, is_oneway);

      // TODO check signature
      return detail::deserialize_and_return<return_type>::eval(msg.get());
   }


   /// asynchronous call
   template<typename... T>
   detail::InterimCallbackHolder<holder_type> async(const T&... t)
   {
      static_assert(is_oneway == false, "it's a oneway function");
      static_assert(std::is_convertible<typename detail::canonify<std::tuple<typename std::decay<T>::type...>>::type,
                    args_type>::value, "args mismatch");

      return detail::InterimCallbackHolder<holder_type>(parent_->send_request(method_name_, [&](DBusMessageIter& s){
         serializer_type::eval(s, t...);
      }, false));
   }


   ClientMethod& operator[](int flags)
   {
      static_assert(is_oneway == false, "it's a oneway function");

      if (flags & (1<<0))
         detail::request_specific_timeout = timeout.timeout_;

      return *this;
   }


private:

   const char* method_name_;
   StubBase* parent_;
};


// ----------------------------------------------------------------------------------------


}   // namespace dbus

}   // namespace simppl


template<typename HolderT, typename FunctorT>
inline
simppl::dbus::PendingCall operator>>(simppl::dbus::detail::InterimCallbackHolder<HolderT>&& r, const FunctorT& f)
{
   // TODO static_assert FunctorT and HolderT::f_ convertible?
   dbus_pending_call_set_notify(r.pc_.pending(), &HolderT::pending_notify, new HolderT(f), &HolderT::_delete);

   return std::move(r.pc_);
}


template<typename DataT, int Flags, typename FuncT>
inline
void operator>>(simppl::dbus::ClientProperty<DataT, Flags>& attr, const FuncT& func)
{
   attr.f_ = func;
}


template<typename... T, typename FuncT>
inline
void operator>>(simppl::dbus::ClientSignal<T...>& sig, const FuncT& func)
{
   sig.f_ = func;
}


#include "simppl/stub.h"

#endif   // SIMPPL_CLIENTSIDE_H
