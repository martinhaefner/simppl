#ifndef SIMPPL_SERVERSIDE_H
#define SIMPPL_SERVERSIDE_H

#include <set>
#include <iostream>

#include <dbus/dbus.h>

#include "simppl/calltraits.h"
#include "simppl/callstate.h"
#include "simppl/property.h"
#include "simppl/skeletonbase.h"
#include "simppl/serialization.h"
#include "simppl/parameter_deduction.h"
#include "simppl/serverrequestdescriptor.h"

#include "simppl/detail/serverresponseholder.h"
#include "simppl/detail/basicinterface.h"
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

// forward decl
namespace detail
{
   struct ServerRequestBaseSetter;
}


struct ServerRequestBase
{
   friend struct detail::ServerRequestBaseSetter;

   virtual void eval(DBusMessage* msg) = 0;

#if SIMPPL_HAVE_INTROSPECTION
   virtual void introspect(std::ostream& os) = 0;
#endif

   ServerRequestBase* next_;
   const char* name_;

protected:

   ServerRequestBase(const char* name, detail::BasicInterface* iface);

   inline
   ~ServerRequestBase()
   {
      // NOOP
   }
};


// ----------------------------------------------------------------------------------


struct ServerSignalBase
{
   ServerSignalBase(const char* name, detail::BasicInterface* iface);

#if SIMPPL_HAVE_INTROSPECTION
   virtual void introspect(std::ostream& os) = 0;
   ServerSignalBase* next_;   // list hook
#endif

protected:

   inline ~ServerSignalBase()
   {
      // NOOP
   }

   const char* name_;
   detail::BasicInterface* parent_;
};


template<typename... T>
struct ServerSignal : ServerSignalBase
{
   static_assert(detail::isValidType<T...>::value, "invalid type in interface");

   inline
   ServerSignal(const char* name, detail::BasicInterface* iface)
    : ServerSignalBase(name, iface)
   {
      // NOOP
   }

   void emit(typename CallTraits<T>::param_type... args)
   {
      if (parent_->conn_)
      {
         SkeletonBase* skel = dynamic_cast<SkeletonBase*>(parent_);
         message_ptr_t msg = make_message(dbus_message_new_signal(skel->objectpath(), skel->iface(), name_));

         detail::Serializer s(msg.get());
         serialize(s, args...);

         dbus_connection_send(parent_->conn_, msg.get(), nullptr);
      }
   }

#if SIMPPL_HAVE_INTROSPECTION
   void introspect(std::ostream& os)
   {
      os << "    <signal name=\"" << this->name_ << "\">";
      detail::introspect_args<T...>(os);
      os << "    </signal>\n";
   }
#endif
};


// -------------------------------------------------------------------------------------


template<typename... ArgsT>
struct ServerRequest : ServerRequestBase
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

    typedef typename detail::generate_server_callback_function<ArgsT...>::type callback_type;

    static_assert(!is_oneway || is_oneway && std::is_same<return_type, void>::value, "oneway check");


    inline
    ServerRequest(const char* name, detail::BasicInterface* iface)
    : ServerRequestBase(name, iface)
    {
        // NOOP
    }

    template<typename FunctorT>
    inline
    void handled_by(FunctorT func)
    {
        assert(!f_);
        f_ = func;
    }

   void eval(DBusMessage* msg)
   {
       assert(f_);
       detail::Deserializer d(msg);
       detail::GetCaller<args_type>::type::template eval(d, f_);
   }

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
   void introspect(std::ostream& os)
   {
      os << "    <method name=\"" << this->name_ << "\">";
      detail::introspect_args<ArgsT...>(os);
      os << "    </method>\n";
   }
#endif

private:

   template<typename... T>
   inline
   detail::ServerResponseHolder __impl(std::true_type, const T&... t)
   {
      std::function<void(detail::Serializer&)> f(std::bind(&simppl::dbus::detail::serialize<const T&...>, std::placeholders::_1, t...));
      return detail::ServerResponseHolder(f);
   }

   template<typename... T>
   inline
   detail::ServerResponseHolder __impl(std::false_type, const T&... t)
   {
      std::function<void(detail::Serializer&)> f;
      return detail::ServerResponseHolder(f);
   }

   callback_type f_;
};


// -----------------------------------------------------------------------------------------


struct ServerPropertyBase
{
   ServerPropertyBase(const char* name, detail::BasicInterface* iface);

   virtual void eval(DBusMessage* msg) = 0;
   virtual void evalSet(detail::Deserializer& ds) = 0;

#if SIMPPL_HAVE_INTROSPECTION
   virtual void introspect(std::ostream& os) = 0;
#endif

   ServerPropertyBase* next_;   ///< list hook
   const char* name_;

protected:

   inline
   ~ServerPropertyBase()
   {
      // NOOP
   }
};


template<typename DataT>
struct BaseProperty : ServerPropertyBase
{
   inline
   BaseProperty(const char* name, detail::BasicInterface* iface)
    : sig_(name, iface)
    , ServerPropertyBase(name, iface)
   {
      // NOOP
   }

   inline
   const DataT& value() const
   {
      return t_;
   }

   void eval(DBusMessage* response)
   {
      detail::Serializer s(response);

      Variant<DataT> v(t_);   // FIXME this copy is overhead, just somehow wrap it...
      serialize(s, v);
   }

protected:

   DataT t_;
   ServerSignal<DataT> sig_;
};


template<typename DataT, int Flags>
struct ServerProperty : BaseProperty<DataT>
{
   static_assert(detail::isValidType<DataT>::value, "invalid type in interface");

   inline
   ServerProperty(const char* name, detail::BasicInterface* iface)
    : BaseProperty<DataT>(name, iface)
   {
      // NOOP
   }

   ServerProperty& operator=(const DataT& data)
   {
      // FIXME if emitting...
      this->sig_.emit(data);

      this->t_ = data;
      return *this;
   }

protected:

   // FIXME only do something if readwrite enabled!
   void evalSet(detail::Deserializer& ds)
   {
      Variant<DataT> v;
      ds >> v;

      *this = *v.template get<DataT>();
   }

#if SIMPPL_HAVE_INTROSPECTION
   void introspect(std::ostream& os)
   {
      // FIXME name_ seems to be here multiple times: signal and ABase
      os << "    <property name=\"" << ServerPropertyBase::name_ << "\" type=\"";
      detail::make_type_signature<DataT>::eval(os);
      os << "\" access=\"" << (Flags & ReadWrite?"readwrite":"read") << "\"/>\n";
   }
#endif
};


}   // namespace dbus

}   // namespace simppl


template<typename FunctorT, typename... T>
inline
void operator>>(simppl::dbus::ServerRequest<T...>& r, const FunctorT& f)
{
   r.handled_by(f);
}


#endif   // SIMPPL_SERVERSIDE_H
