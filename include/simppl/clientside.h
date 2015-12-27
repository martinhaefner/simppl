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
#include "simppl/detail/parented.h"
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
   virtual void eval(DBusMessage* msg) = 0;
   
protected:

   inline
   ~ClientResponseBase()
   {
      // NOOP
   }
};


// ---------------------------------------------------------------------------------


struct ClientSignalBase : detail::Parented
{
   virtual void eval(const void* data, size_t len) = 0;
   
   inline
   ClientSignalBase(const char* name, void* parent)
    : detail::Parented(parent)
    , name_(name)
   {
      // NOOP
   }

   
protected:
   
   inline
   ~ClientSignalBase()
   {
      // NOOP
   }
   
   const char* name_;   
};


template<typename... T>
struct ClientSignal : ClientSignalBase
{
   static_assert(detail::isValidType<T...>::value, "invalid type in interface");
   
   typedef std::function<void(typename CallTraits<T>::param_type...)> function_type;
      
   inline
   ClientSignal(const char* name, void* parent)
    : ClientSignalBase(name, parent)
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
      parent<StubBase>()->sendSignalRegistration(*this);
      return *this;
   }
   
   /// send de-registration to the server - only attach after the interface is connected.
   inline
   ClientSignal& detach()
   {
      parent<StubBase>()->sendSignalUnregistration(*this);
      return *this;
   }
   
   
   void eval(const void* payload, size_t length)
   {
      if (f_)
      {
         detail::Deserializer d(payload, length);
         detail::GetCaller<T...>::type::template eval(d, f_);
      }
      else
         std::cerr << "No appropriate handler registered for signal " << name_ << " with payload size=" << length << std::endl;
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
   ClientAttribute(const char* name, std::vector<detail::Parented*>& parent)
    : signal_(name, parent)
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
struct ClientRequest : detail::Parented
{
   static_assert(detail::isValidType<T...>::value, "invalid_type_in_interface");
      
   inline
   ClientRequest(const char* method_name, void* parent)
    : detail::Parented(parent)
    , method_name_(method_name)
    , handler_(0)
   {
      // NOOP
   }
      
   inline
   detail::ClientResponseHolder operator()(typename CallTraits<T>::param_type... t)
   {
      DBusMessage* msg = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
      DBusPendingCall* pending = nullptr;
      
      detail::Serializer s(*msg);
      serialize(s, t...);
      
      if (handler_)
      {
         dbus_connection_send_with_reply(this->conn_, msg, &pending, detail::request_specific_timeout);
      }
      else
         dbus_connection_send(this->conn_, msg, nullptr);
         
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

   ClientResponseBase* handler_;
   const char* method_name_;
};


// ----------------------------------------------------------------------------------------


template<typename... T>
struct ClientResponse : ClientResponseBase
{ 
   static_assert(detail::isValidType<T...>::value, "invalid_type_in_interface");
   
   typedef std::function<void(const CallState&, typename CallTraits<T>::param_type...)> function_type;
   
   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      f_ = func;
   }
   
   void eval(DBusMessage* msg)
   {
      if (f_)
      {
//FIXME         detail::Deserializer d(payload, length);
//         detail::GetCaller<T...>::type::template evalResponse(d, f_, cs);
      }
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
