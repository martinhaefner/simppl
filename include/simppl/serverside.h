#ifndef SIMPPL_SERVERSIDE_H
#define SIMPPL_SERVERSIDE_H


#include <set>
#include <iostream>

#include <dbus/dbus.h>

#include "simppl/calltraits.h"
#include "simppl/callstate.h"
#include "simppl/attribute.h"
#include "simppl/skeletonbase.h"
#include "simppl/serverrequestdescriptor.h"
#include "simppl/variant.h"

#include "simppl/detail/serverresponseholder.h"
#include "simppl/detail/basicinterface.h"
#include "simppl/detail/validation.h"


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

   inline
   bool hasResponse() const
   {
      return hasResponse_;
   }

protected:

   ServerRequestBase(const char* name, detail::BasicInterface* iface);

   inline
   ~ServerRequestBase()
   {
      // NOOP
   }

   bool hasResponse_;
   const char* name_;
};


struct ServerResponseBase
{
   std::set<ServerRequestBase*> allowedRequests_;

protected:

   inline
   ~ServerResponseBase()
   {
      // NOOP
   }
};


// ----------------------------------------------------------------------------------


template<typename... T>
struct ServerSignal
{
   static_assert(detail::isValidType<T...>::value, "invalid type in interface");

   inline
   ServerSignal(const char* name, detail::BasicInterface* iface)
    : name_(name)
    , parent_(iface)
   {
      // NOOP
   }

   inline
   void emit(typename CallTraits<T>::param_type... args)
   {
      if (parent_->conn_)
      {
         SkeletonBase* skel = dynamic_cast<SkeletonBase*>(parent_);
         DBusMessage* msg = dbus_message_new_signal(skel->objectpath(), skel->iface(), name_);

         detail::Serializer s(msg);
         serialize(s, args...);

         dbus_connection_send(parent_->conn_, msg, nullptr);
         dbus_message_unref(msg);
      }
   }


protected:

   const char* name_;
   detail::BasicInterface* parent_;
};


// -------------------------------------------------------------------------------------


template<typename... T>
struct ServerRequest : ServerRequestBase
{
   static_assert(detail::isValidType<T...>::value, "invalid_type_in_interface");

   typedef std::function<void(typename CallTraits<T>::param_type...)> function_type;

   inline
   ServerRequest(const char* name, detail::BasicInterface* iface)
    : ServerRequestBase(name, iface)
   {
      // NOOP
   }

   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      assert(!f_);
      f_ = func;
   }

   void eval(DBusMessage* msg)
   {
      if (f_)
      {
          detail::Deserializer d(msg);
          detail::GetCaller<T...>::type::template eval(d, f_);
      }
      else
         assert(false);    // no response handler registered
   }

   function_type f_;
};


// ---------------------------------------------------------------------------------------------


template<typename... T>
struct ServerResponse : ServerResponseBase
{
   static_assert(detail::isValidType<T...>::value, "invalid_type_in_interface");

   inline
   ServerResponse(const char* /*name*/, detail::BasicInterface* iface)
    : iface_(iface)
   {
      // NOOP
   }

   inline
   detail::ServerResponseHolder operator()(typename CallTraits<T>::param_type... t)
   {
      return __impl(bool_<(sizeof...(t) > 0)>(), t...);
   }


private:

   inline
   detail::ServerResponseHolder __impl(tTrueType, typename CallTraits<T>::param_type... t)
   {
      std::function<void(detail::Serializer&)> f(std::bind(&simppl::dbus::detail::serialize<typename CallTraits<T>::param_type...>, std::placeholders::_1, t...));
      return detail::ServerResponseHolder(*this, f);
   }

   inline
   detail::ServerResponseHolder __impl(tFalseType, typename CallTraits<T>::param_type... t)
   {
      std::function<void(detail::Serializer&)> f;
      return detail::ServerResponseHolder(*this, f);
   }

   detail::BasicInterface* iface_;
};


// -----------------------------------------------------------------------------------------


struct ServerAttributeBase
{
   ServerAttributeBase(const char* name, detail::BasicInterface* iface);

   virtual void eval(DBusMessage* msg) = 0;
   virtual void evalSet(detail::Deserializer& ds) = 0;

protected:

   inline
   ~ServerAttributeBase()
   {
      // NOOP
   }

   const char* name_;
};


template<typename DataT>
struct BaseAttribute : ServerSignal<DataT>, ServerAttributeBase
{
   inline
   BaseAttribute(const char* name, detail::BasicInterface* iface)
    : ServerSignal<DataT>(name, iface)
    , ServerAttributeBase(name, iface)
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
};


template<typename DataT, int Flags>
struct ServerAttribute : BaseAttribute<DataT>
{
   static_assert(detail::isValidType<DataT>::value, "invalid type in interface");

   inline
   ServerAttribute(const char* name, detail::BasicInterface* iface)
    : BaseAttribute<DataT>(name, iface)
   {
      // NOOP
   }

   inline
   ServerAttribute& operator=(const DataT& data)
   {
      // FIXME if emitting...
      this->emit(data);

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
};


}   // namespace dbus

}   // namespace simppl


template<typename FunctorT, typename... T>
inline
void operator>>(simppl::dbus::ServerRequest<T...>& r, const FunctorT& f)
{
   r.handledBy(f);
}


#endif   // SIMPPL_SERVERSIDE_H
