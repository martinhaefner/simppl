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

#include "simppl/detail/validation.h"
#include "simppl/detail/serverresponseholder.h"
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

namespace detail
{
    // forward decl
    template<bool Notifying, int Flags>
    struct Assigner;
}


struct ServerMethodBase
{
   typedef void(*eval_type)(ServerMethodBase*, DBusMessage*);
   typedef message_ptr_t(*throw_type)(ServerMethodBase*, DBusMessage&, const Error&);
   typedef void(*sig_type)(std::ostream&);

   void eval(DBusMessage* msg)
   {
      eval_(this, msg);
   }

   message_ptr_t _throw(DBusMessage& msg, const Error& err)
   {
       return throw_(this, msg, err);
   }


#if SIMPPL_HAVE_INTROSPECTION
   virtual void introspect(std::ostream& os) const = 0;
#endif

   ServerMethodBase* next_;
   const char* name_;

protected:

   ServerMethodBase(const char* name, SkeletonBase* iface, int iface_id);

   ~ServerMethodBase();

   eval_type eval_;
   throw_type throw_;
};


// ----------------------------------------------------------------------------------


struct ServerSignalBase
{
   ServerSignalBase(const char* name, SkeletonBase* iface, int iface_id);

#if SIMPPL_HAVE_INTROSPECTION
   virtual void introspect(std::ostream& os) const = 0;
   ServerSignalBase* next_;   // list hook
#endif

protected:

   ~ServerSignalBase() = default;

   const char* name_;
   int iface_id_;
   SkeletonBase* parent_;
};


template<typename... T>
struct ServerSignal : ServerSignalBase
{
   ServerSignal(const char* name, SkeletonBase* iface, int iface_id)
    : ServerSignalBase(name, iface, iface_id)
   {
      // NOOP
   }

   void notify(typename CallTraits<T>::param_type... args)
   {
       parent_->send_signal(this->name_, this->iface_id_, [&](DBusMessageIter& iter){
            encode(iter, args...);
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
        valid     = AllOf<typename make_typelist<ArgsT...>::type, detail::InOutThrowOrOneway>::value,
        is_oneway = detail::is_oneway_request<ArgsT...>::value,
    };

    typedef typename detail::canonify<typename args_type_generator::type>::type                   args_type;
    typedef typename detail::canonify<typename return_type_generator::type>::type                 return_type;

    typedef typename detail::generate_server_callback_function<ArgsT...>::type                    callback_type;
    typedef typename detail::generate_serializer<typename return_type_generator::list_type>::type serializer_type;

    typedef typename detail::get_exception_type<ArgsT...>::type                                   exception_type;

    static_assert(!is_oneway || (is_oneway && std::is_same<return_type, void>::value), "oneway check");


    inline
    ServerMethod(const char* name, SkeletonBase* iface, int iface_id)
    : ServerMethodBase(name, iface, iface_id)
    {
        eval_ = __eval;
        throw_ = __throw<exception_type>;
    }


   template<typename... T>
   detail::ServerResponseHolder operator()(const T&... t)
   {
      static_assert(is_oneway == false, "it's a oneway function");
      static_assert(std::is_convertible<typename detail::canonify<std::tuple<typename std::decay<T>::type...>>::type,
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

       detail::GetCaller<args_type>::type::template eval<>(iter, ((ServerMethod*)obj)->f_);
   }


   template<typename T>
   static
   message_ptr_t __throw(ServerMethodBase* /*obj*/, DBusMessage& req, const Error& err)
   {
       return detail::ErrorFactory<T>::reply(req, err);
   }


   template<typename... T>
   detail::ServerResponseHolder __impl(std::true_type, const T&... t)
   {
      return detail::ServerResponseHolder([&](DBusMessageIter& s){
         serializer_type::eval(s, t...);
      });
   }

   template<typename... T>
   detail::ServerResponseHolder __impl(std::false_type /*f*/, const T&... /*t*/)
   {
      return detail::ServerResponseHolder([](DBusMessageIter&){ /*NOOP*/ });
   }
};


// -----------------------------------------------------------------------------------------


struct ServerPropertyBase
{
   typedef void (*eval_type)(ServerPropertyBase*, DBusMessageIter*);
   typedef void (*eval_set_type)(ServerPropertyBase*, DBusMessageIter&);

   ServerPropertyBase(const char* name, SkeletonBase* iface, int iface_id);

   void eval(DBusMessageIter* iter)
   {
      return eval_(this, iter);
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
   int iface_id_;
   SkeletonBase* parent_;

protected:

   ~ServerPropertyBase() = default;

   eval_type eval_;
   eval_set_type eval_set_;
};


template<typename DataT>
struct BaseProperty : ServerPropertyBase
{
   template<bool Notifying, int Flags> friend struct detail::Assigner;

   typedef std::function<DataT()> cb_type;

   BaseProperty(const char* name, SkeletonBase* iface, int iface_id)
    : ServerPropertyBase(name, iface, iface_id)
    , t_()
   {
      eval_ = __eval;
   }

   const DataT& value() const
   {
       return std::get<DataT>(t_);
   }

   static
   void __eval(ServerPropertyBase* obj, DBusMessageIter* iter)
   {
       auto that = ((BaseProperty*)obj);

       detail::PropertyCodec<DataT>::encode(*iter, std::get_if<cb_type>(&that->t_) ? (std::get<cb_type>(that->t_))() : std::get<DataT>(that->t_));
   }

   /**
    * Shall be used with properties implementing on_read callback in order
    * to send property changes. Not suitable with value holding properties.
    */
   void notify(const DataT& data)
   {
      this->parent_->send_property_change(this->name_, this->iface_id_, [this, data](DBusMessageIter& iter){
         detail::PropertyCodec<DataT>::encode(iter, data);
      });
   }

   /**
    * Send an invalidation message, shall only be used in conjunction with the notify message and
    * the on_read callback. Currently, value holding properties cannot be invalidated.
    */
   void invalidate()
   {
      this->parent_->send_property_invalidate(this->name_, this->iface_id_);
   }

   /// set callback for each read
   void on_read(cb_type cb)
   {
       t_ = cb;
   }

protected:

   std::variant<DataT, cb_type> t_;
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


namespace detail
{
    // do not instantiate the comparison operator if not notifying!
    template<bool Notifying, int Flags>
    struct Assigner
    {
        template<typename DataT>
        static
        void eval(BaseProperty<DataT>& p, const DataT& d)
        {
            p.t_ = d;
        }
    };


    template<int Flags>
    struct Assigner<true, Flags>
    {
        template<typename DataT>
        static
        void eval(BaseProperty<DataT>& p, const DataT& d)
        {
            if (!std::get_if<DataT>(&p.t_) || PropertyComparator<DataT, (Flags & Always ? false : true)>::compare(p.value(), d))
            {
                p.t_ = d;
                p.notify(d);
            }
        }
    };
}


template<typename DataT, int Flags>
struct ServerProperty : BaseProperty<DataT>, std::conditional<Flags & ReadWrite, ServerWritableMixin<DataT>, ServerNoopMixin<DataT>>::type
{
   ServerProperty(const char* name, SkeletonBase* iface, int iface_id)
    : BaseProperty<DataT>(name, iface, iface_id)
   {
      if (Flags & ReadWrite)
         this->eval_set_ = __eval_set;
   }

   ServerProperty& operator=(const DataT& data)
   {
      detail::Assigner<Flags & Notifying ? true : false, Flags>::eval(*this, data);
      return *this;
   }


protected:

    static
    void __eval_set(ServerPropertyBase* obj, DBusMessageIter& iter)
    {
        ServerProperty* that = (ServerProperty*)obj;

        DataT t;
        detail::PropertyCodec<DataT>::decode(iter, t);

        if (that->__set(t))
            *that = std::move(t);
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
