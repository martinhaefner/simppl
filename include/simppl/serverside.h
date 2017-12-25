#ifndef SIMPPL_SERVERSIDE_H
#define SIMPPL_SERVERSIDE_H


#include <dbus/dbus.h>

#include "simppl/calltraits.h"
#include "simppl/callstate.h"
#include "simppl/property.h"
#include "simppl/skeletonbase.h"
#include "simppl/serialization.h"
#include "simppl/parameter_deduction.h"
#include "simppl/serverrequestdescriptor.h"

#include "simppl/detail/serverresponseholder.h"
#include "simppl/detail/validation.h"
#include "simppl/detail/callinterface.h"


#if !(defined(SIMPPL_SKELETONBASE_H) \
      || defined(SIMPPL_DISPATCHER_CPP) \
      || defined(SIMPPL_SERVERSIDE_CPP) \
      || defined(SIMPPL_SKELETONBASE_CPP) \
      )
#   error "Don't include this file manually. Include 'skeleton.h' instead".
#endif


namespace simppl
{

namespace dbus
{

struct ServerMethodBase
{
   typedef void(*eval_type)(ServerMethodBase*, DBusMessage*);
   typedef void(*sig_type)(std::ostream&);

   void eval(DBusMessage* msg)
   {
      eval_(this, msg);
   }

#if SIMPPL_SIGNATURE_CHECK
   void get_signature(std::ostream& os) const
   {
      sig_(os);
   }

   const char* get_signature() const;
#endif

#if SIMPPL_HAVE_INTROSPECTION
   virtual void introspect(std::ostream& os) const = 0;
#endif

   ServerMethodBase* next_;
   const char* name_;

protected:

   ServerMethodBase(const char* name, SkeletonBase* iface);

   ~ServerMethodBase();

   eval_type eval_;

#if SIMPPL_SIGNATURE_CHECK
   mutable std::string signature_;
   sig_type sig_;
#endif
};


// ----------------------------------------------------------------------------------


struct ServerSignalBase
{
   ServerSignalBase(const char* name, SkeletonBase* iface);

#if SIMPPL_HAVE_INTROSPECTION
   virtual void introspect(std::ostream& os) const = 0;
   ServerSignalBase* next_;   // list hook
#endif

protected:

   inline
   ~ServerSignalBase() = default;

   const char* name_;
   SkeletonBase* parent_;
};


template<typename... T>
struct ServerSignal : ServerSignalBase
{
   static_assert(detail::isValidType<T...>::value, "invalid type in interface");

   inline
   ServerSignal(const char* name, SkeletonBase* iface)
    : ServerSignalBase(name, iface)
   {
      // NOOP
   }

   void notify(typename CallTraits<T>::param_type... args)
   {
       parent_->send_signal(this->name_, [&](DBusMessageIter& iter){
            detail::serialize(iter, args...);
       });
   }

#if SIMPPL_HAVE_INTROSPECTION
   void introspect(std::ostream& os) const override
   {
      os << "    <signal name=\"" << this->name_ << "\">";
      detail::introspect_args<T...>(os);
      os << "    </signal>\n";
   }
#endif
};


// -------------------------------------------------------------------------------------


template<typename... ArgsT>
struct ServerMethod : ServerMethodBase
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

    typedef typename detail::generate_server_callback_function<ArgsT...>::type                     callback_type;
    typedef typename detail::generate_serializer<typename return_type_generator::list_type>::type serializer_type;

    static_assert(!is_oneway || (is_oneway && std::is_same<return_type, void>::value), "oneway check");


    inline
    ServerMethod(const char* name, SkeletonBase* iface)
    : ServerMethodBase(name, iface)
    {
        eval_ = __eval;
#if SIMPPL_SIGNATURE_CHECK
        sig_ = __sig;
#endif
    }

#if SIMPPL_SIGNATURE_CHECK
   static
   void __sig(std::ostream& os)
   {
      ForEach<typename args_type_generator::list_type>::template eval<detail::make_type_signature>(os);
   }
#endif

   template<typename... T>
   inline
   detail::ServerResponseHolder operator()(const T&... t)
   {
      static_assert(is_oneway == false, "it's a oneway function");
      static_assert(std::is_same<typename detail::canonify<std::tuple<typename std::decay<T>::type...>>::type,
                    return_type>::value, "args mismatch");

      return __impl(bool_constant<(sizeof...(t) > 0)>(), t...);
   }

#if SIMPPL_HAVE_INTROSPECTION
   void introspect(std::ostream& os) const override
   {
      os << "    <method name=\"" << this->name_ << "\">";
      detail::introspect_args<ArgsT...>(os);
      os << "    </method>\n";
   }
#endif


   callback_type f_;
   
private:

   static
   void __eval(ServerMethodBase* obj, DBusMessage* msg)
   {
       DBusMessageIter iter;
       dbus_message_iter_init(msg, &iter);
       
       detail::GetCaller<args_type>::type::template eval(iter, ((ServerMethod*)obj)->f_);
   }


   template<typename... T>
   inline
   detail::ServerResponseHolder __impl(std::true_type, const T&... t)
   {
      return detail::ServerResponseHolder([&](DBusMessageIter& s){
         serializer_type::eval(s, t...);
      });
   }

   template<typename... T>
   inline
   detail::ServerResponseHolder __impl(std::false_type, const T&... t)
   {
      return detail::ServerResponseHolder([](DBusMessageIter&){ /*NOOP*/ });
   }
};


// -----------------------------------------------------------------------------------------


struct ServerPropertyBase
{
   typedef void (*eval_type)(ServerPropertyBase*, DBusMessage*);
   typedef void (*eval_set_type)(ServerPropertyBase*, DBusMessageIter&);

   ServerPropertyBase(const char* name, SkeletonBase* iface);

   void eval(DBusMessage* msg)
   {
      eval_(this, msg);
   }

   void evalSet(DBusMessageIter& iter)
   {
      eval_set_(this, iter);
   }

#if SIMPPL_HAVE_INTROSPECTION
   virtual void introspect(std::ostream& os) const = 0;
#endif

   ServerPropertyBase* next_;   ///< list hook
   const char* name_;
   SkeletonBase* parent_;

protected:

   inline
   ~ServerPropertyBase() = default;

   eval_type eval_;
   eval_set_type eval_set_;
};


template<typename DataT>
struct BaseProperty : ServerPropertyBase
{
   inline
   BaseProperty(const char* name, SkeletonBase* iface)
    : ServerPropertyBase(name, iface)
    , t_()
   {
      eval_ = __eval;
   }

   inline
   const DataT& value() const
   {
      return t_;
   }

   static
   void __eval(ServerPropertyBase* obj, DBusMessage* response)
   {
      DBusMessageIter iter;
      dbus_message_iter_init_append(response, &iter);

      Variant<DataT> v(((BaseProperty*)obj)->t_);   // FIXME this copy is overhead, just somehow wrap it...
      detail::serialize(iter, v);
   }

protected:

   DataT t_;
};


template<typename DataT>
struct ServerNoopMixin
{
    bool __set(typename CallTraits<DataT>::param_type)
    {
        return true;
    }
};


template<typename DataT>
struct ServerWritableMixin
{
    typedef std::function<void(typename CallTraits<DataT>::param_type)> function_type;
    
    /**
     * A writable server attribute may automatically overwrite the stored
     * value if no callback is provided. If a callback function is provided,
     * the callback will be called and has to decide if it will overwrite
     * the attribute member (by calling operator=) or just negogiate the
     * setter call. 
     */
    bool __set(typename CallTraits<DataT>::param_type d)
    {
        if (f_)
        {
            f_(d);
            return false;
        }
        
        return true;
    }
    
    function_type f_;
};


template<typename DataT, int Flags>
struct ServerProperty : BaseProperty<DataT>, std::conditional<Flags & ReadWrite, ServerWritableMixin<DataT>, ServerNoopMixin<DataT>>::type
{
   static_assert(detail::isValidType<DataT>::value, "invalid type in interface");

   inline
   ServerProperty(const char* name, SkeletonBase* iface)
    : BaseProperty<DataT>(name, iface)
   {
      if (Flags & ReadWrite)
         this->eval_set_ = __eval_set;
   }

   ServerProperty& operator=(const DataT& data)
   {
      if (PropertyComparator<DataT, (Flags & Always ? false : true)>::compare(this->t_, data))
      {
         this->t_ = data;

         if (Flags & Notifying)
         {
            this->parent_->send_property_change(this->name_, [this](DBusMessageIter& iter){
               detail::serialize_property(iter, this->t_);
            });
         }
      }

      return *this;
   }


protected:

    static
    void __eval_set(ServerPropertyBase* obj, DBusMessageIter& iter)
    {
        ServerProperty* that = (ServerProperty*)obj;

        Variant<DataT> v;
        Codec<decltype(v)>::decode(iter, v);
        
        if (that->__set(*v.template get<DataT>()))
            *that = std::move(*v.template get<DataT>());
    }


#if SIMPPL_HAVE_INTROSPECTION
   void introspect(std::ostream& os) const override
   {
      os << "    <property name=\"" << this->name_ << "\" type=\"";
      Codec<DataT>::make_type_signature(os);
      os << "\" access=\"" << (Flags & ReadWrite?"readwrite":"read") << "\"/>\n";
   }
#endif
};


}   // namespace dbus

}   // namespace simppl


template<typename FunctorT, typename... T>
inline
void operator>>(simppl::dbus::ServerMethod<T...>& r, const FunctorT& f)
{
    r.f_ = f;
}


template<typename FunctorT, typename DataT, int Flags>
inline
void operator>>(simppl::dbus::ServerProperty<DataT, Flags>& p, const FunctorT& f)
{
    p.f_ = f;
}


#endif   // SIMPPL_SERVERSIDE_H
