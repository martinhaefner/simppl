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

#include "simppl/detail/callinterface.h"
#include "simppl/detail/validation.h"
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
   typedef void (*eval_type)(ClientSignalBase*, Decoder&);

   template<typename, int>
   friend struct ClientProperty;
   friend struct StubBase;

   void eval(Decoder& d)
   {
      eval_(this, d);
   }

   ClientSignalBase(const char* name, StubBase* iface, int iface_id);

   const char* name() const
   {
       return name_;
   }


protected:

   ~ClientSignalBase() = default;

   StubBase* stub_;
   const char* name_;

   eval_type eval_;

   ClientSignalBase* next_;
};


template<typename... T>
struct ClientSignal : ClientSignalBase
{
   typedef typename std::conditional<sizeof...(T) == 0, void, std::tuple<T...>>::type args_type;

   typedef std::function<void(typename CallTraits<T>::param_type...)> function_type;

   ClientSignal(const char* name, StubBase* iface, int iface_id)
    : ClientSignalBase(name, iface, iface_id)
   {
      eval_ = __eval;
   }

   template<typename FuncT>
   void set_callback(const FuncT& f)
   {
      f_ = f;
   }

   /// send registration to the server - only attach after the interface is connected.
   ClientSignal& attach()
   {
      stub_->register_signal(*this);
      return *this;
   }

   /// send de-registration to the server - only attach after the interface is connected.
   ClientSignal& detach()
   {
      stub_->unregister_signal(*this);
      return *this;
   }


private:

   static
   void __eval(ClientSignalBase* obj, Decoder& d)
   {
      detail::GetCaller<args_type>::type::template eval(d, ((ClientSignal*)(obj))->f_);
   }

   function_type f_;
};


// ---------------------------------------------------------------------------------------------


struct ClientPropertyBase
{
   friend struct StubBase;

   /// Decoder nullptr: the property was invalidated
   typedef void (*eval_type)(ClientPropertyBase*, Decoder*);


   ClientPropertyBase(const char* name, StubBase* iface, int iface_id);

   void eval(Decoder* d)
   {
      eval_(this, d);
   }

   /// only call this after the server is connected.
   void detach();


protected:

   ~ClientPropertyBase() = default;

   const char* name_;
   StubBase* stub_;

   eval_type eval_;
};


namespace detail
{

/**
 * Property access with request specific options, see ClientProperty::operator[].
 */
template<typename PropertyT>
struct ClientPropertyWithOptions
{
   typename PropertyT::data_type get()
   {
      return property_.get(opts_);
   }

   auto get_async()
   {
      return property_.get_async(opts_);
   }

   /// writable properties only
   void set(typename PropertyT::arg_type t)
   {
      property_.set(opts_, t);
   }

   /// writable properties only
   auto set_async(typename PropertyT::arg_type t)
   {
      return property_.set_async(opts_, t);
   }

   /// writable properties only
   ClientPropertyWithOptions& operator=(typename PropertyT::arg_type t)
   {
      set(t);
      return *this;
   }

   PropertyT& property_;
   RequestOptions opts_;
};

}   // namespace detail


template<typename PropertyT, typename DataT>
struct ClientPropertyWritableMixin : ClientPropertyBase
{
   template<typename> friend struct detail::ClientPropertyWithOptions;

   typedef DataT data_type;
   typedef typename CallTraits<DataT>::param_type arg_type;
   typedef std::function<void(const CallState&)> function_type;
   typedef detail::CallbackHolder<function_type, void, Error> holder_type;


   ClientPropertyWritableMixin(const char* name, StubBase* iface, int iface_id)
    : ClientPropertyBase(name, iface, iface_id)
   {
      // NOOP
   }

   /// blocking version
   void set(arg_type t)
   {
      set(RequestOptions(), t);
   }


   /// async version
   detail::InterimCallbackHolder<holder_type> set_async(arg_type t)
   {
      return set_async(RequestOptions(), t);
   }

private:

   void set(const RequestOptions& opts, arg_type t)
   {
      auto that = (PropertyT*)this;

      that->stub_->set_property(that->name_, [&t](DBusMessageIter& s){
         Encoder e(s);
         detail::PropertyCodec<data_type>::encode(e, t);
      }, opts.timeout_);
   }


   detail::InterimCallbackHolder<holder_type> set_async(const RequestOptions& opts, arg_type t)
   {
      auto that = (PropertyT*)this;

      return detail::InterimCallbackHolder<holder_type>(that->stub_->set_property_async(that->name_, [&t](DBusMessageIter& s){
         Encoder e(s);
         detail::PropertyCodec<data_type>::encode(e, t);
      }, opts.timeout_));
   }
};


template<typename DataT, int Flags = Notifying|ReadOnly>
struct ClientProperty
 : std::conditional<(Flags & ReadWrite), ClientPropertyWritableMixin<ClientProperty<DataT, Flags>, DataT>, ClientPropertyBase>::type
{
   template<typename> friend struct detail::ClientPropertyWithOptions;

   typedef typename std::conditional<(Flags & ReadWrite), ClientPropertyWritableMixin<ClientProperty<DataT, Flags>, DataT>, ClientPropertyBase>::type base_type;
   typedef DataT data_type;
   typedef typename CallTraits<DataT>::param_type arg_type;
   typedef std::function<void(const CallState&, arg_type)> function_type;
   typedef ClientSignal<DataT> signal_type;
   typedef detail::PropertyCallbackHolder<std::function<void(const CallState&, arg_type)>, data_type> holder_type;


   ClientProperty(const char* name, StubBase* iface, int iface_id)
    : base_type(name, iface, iface_id)
   {
      this->eval_ = __eval;
   }

   template<typename FuncT>
   void set_callback(const FuncT& f)
   {
      f_ = f;
   }

   /// only call this after the server is connected.
   ClientProperty& attach();

   DataT get()
   {
      return get(RequestOptions());
   }

   detail::InterimCallbackHolder<holder_type> get_async()
   {
      return get_async(RequestOptions());
   }


   /// blocking version
   ClientProperty& operator=(arg_type t)
   {
      this->set(t);
      return *this;
   }


   /**
    * Request specific options, only valid for the access directly following:
    *
    *    int i = stub.prop[simppl::dbus::timeout = 700ms].get();
    *    stub.prop[simppl::dbus::timeout = 700ms] = 42;
    */
   detail::ClientPropertyWithOptions<ClientProperty> operator[](const RequestOptions& opts)
   {
      return { *this, opts };
   }


private:

   DataT get(const RequestOptions& opts);

   detail::InterimCallbackHolder<holder_type> get_async(const RequestOptions& opts)
   {
      return detail::InterimCallbackHolder<holder_type>(this->stub_->get_property_async(this->name_, opts.timeout_));
   }

   static
   void __eval(ClientPropertyBase* obj, Decoder* dec)
   {
      ClientProperty* that = (ClientProperty*)obj;

      if (that->f_)
      {
          if (dec)
          {
              data_type d;
              detail::PropertyCodec<data_type>::decode(*dec, d);
              that->f_(CallState(42), d);
          }
          else
              that->f_(CallState(new Error("simppl.dbus.Invalid")), data_type());
      }
   }


   function_type f_;
};


template<typename DataT, int Flags>
DataT ClientProperty<DataT, Flags>::get(const RequestOptions& opts)
{
   message_ptr_t msg = this->stub_->get_property(this->name_, opts.timeout_);

   Decoder d(msg.get());

   DataT t;
   detail::PropertyCodec<DataT>::decode(d, t);

   return t;
}


/// only call this after the server is connected.
template<typename DataT, int Flags>
ClientProperty<DataT, Flags>& ClientProperty<DataT, Flags>::attach()
{
  this->stub_->attach_property(this);

  dbus_pending_call_set_notify(this->stub_->get_property_async(this->name_, std::chrono::milliseconds(0)).pending(),
     &holder_type::pending_notify,
     new holder_type([this](const CallState& cs, const arg_type& val){
        if (f_)
           f_(cs, val);
     }),
     &holder_type::_delete);

  return *this;
}


// --------------------------------------------------------------------------------


struct ClientMethodBase
{
    typedef void(*throw_func_type)(DBusMessage&);

    ClientMethodBase(const char* method_name, StubBase* parent)
     : method_name_(method_name)
     , parent_(parent)
    {
        // NOOP
    }

    void _throw(DBusMessage& msg)
    {
        return throw_(msg);
    }

// FIXME friend or public protected:

   throw_func_type throw_;

   const char* method_name_;
   StubBase* parent_;
};


namespace detail
{

/**
 * A method call with request specific options, see ClientMethod::operator[].
 */
template<typename MethodT>
struct ClientMethodWithOptions
{
   template<typename... T>
   typename MethodT::return_type operator()(const T&... t)
   {
      return method_.call(opts_, t...);
   }

   template<typename... T>
   auto async(const T&... t)
   {
      return method_.call_async(opts_, t...);
   }

   MethodT& method_;
   RequestOptions opts_;
};

}   // namespace detail


template<typename... ArgsT>
struct ClientMethod : ClientMethodBase
{
   template<typename> friend struct detail::ClientMethodWithOptions;

    typedef detail::generate_argument_type<ArgsT...>  args_type_generator;
    typedef detail::generate_return_type<ArgsT...>    return_type_generator;

    enum {
        valid     = AllOf<typename make_typelist<ArgsT...>::type, detail::InOutThrowOrOneway>::value,
        is_oneway = detail::is_oneway_request<ArgsT...>::value
    };

    typedef typename detail::canonify<typename args_type_generator::const_type>::type    args_type;
    typedef typename detail::canonify<typename return_type_generator::type>::type        return_type;

    typedef typename detail::get_exception_type<ArgsT...>::type                          exception_type;
    typedef typename detail::generate_callback_function<exception_type, ArgsT...>::type  callback_type;
    typedef detail::CallbackHolder<callback_type, return_type, exception_type>           holder_type;

    // correct typesafe serializer
    typedef typename detail::generate_serializer<typename args_type_generator::const_list_type>::type serializer_type;

    static_assert(!is_oneway || (is_oneway && std::is_same<return_type, void>::value), "oneway check");


    ClientMethod(const char* method_name, StubBase* parent, int /*iface_id*/)
     : ClientMethodBase(method_name, parent)
    {
        throw_ = __throw;
    }


   /// blocking call
   template<typename... T>
   return_type operator()(const T&... t)
   {
      return call(RequestOptions(), t...);
   }


   /// asynchronous call
   template<typename... T>
   detail::InterimCallbackHolder<holder_type> async(const T&... t)
   {
      return call_async(RequestOptions(), t...);
   }


   /**
    * Request specific options, only valid for the call directly following:
    *
    *    stub.method[simppl::dbus::timeout = 700ms](args...);
    */
   detail::ClientMethodWithOptions<ClientMethod> operator[](const RequestOptions& opts)
   {
      static_assert(is_oneway == false, "it's a oneway function");

      return { *this, opts };
   }

private:

   template<typename... T>
   return_type call(const RequestOptions& opts, const T&... t)
   {
//      std::cout << abi::__cxa_demangle(typeid(typename detail::canonify<std::tuple<T...>>::type).name(), 0, 0, 0) << std::endl;
//      std::cout << abi::__cxa_demangle(typeid(args_type).name(), 0, 0, 0) << std::endl;

      static_assert(std::is_convertible<typename detail::canonify<std::tuple<T...>>::type,
                    args_type>::value, "args mismatch");

      auto msg = parent_->send_request_and_block(this, [&](DBusMessageIter& s){
         Encoder e(s);
         serializer_type::eval(e, t...);
      }, is_oneway, opts.timeout_);

      return detail::deserialize_and_return<return_type>::eval(msg.get());
   }


   template<typename... T>
   detail::InterimCallbackHolder<holder_type> call_async(const RequestOptions& opts, const T&... t)
   {
      static_assert(is_oneway == false, "it's a oneway function");
      static_assert(std::is_convertible<typename detail::canonify<std::tuple<typename std::decay<T>::type...>>::type,
                    args_type>::value, "args mismatch");

      return detail::InterimCallbackHolder<holder_type>(parent_->send_request(this, [&](DBusMessageIter& s){
         Encoder e(s);
         serializer_type::eval(e, t...);
      }, false, opts.timeout_));
   }


   static
   void __throw(DBusMessage& msg)
   {
       exception_type err;
       detail::ErrorFactory<exception_type>::init(err, msg);

       throw err;
   }
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
   attr.set_callback(func);
}


template<typename... T, typename FuncT>
inline
void operator>>(simppl::dbus::ClientSignal<T...>& sig, const FuncT& func)
{
   sig.set_callback(func);
}


#include "simppl/stub.h"

#endif   // SIMPPL_CLIENTSIDE_H
