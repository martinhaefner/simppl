#ifndef SIMPPL_CLIENTSIDE_H
#define SIMPPL_CLIENTSIDE_H


#include <iostream>
#include <functional>

#include "simppl/calltraits.h"
#include "simppl/callstate.h"
#include "simppl/attribute.h"

#include "simppl/detail/validation.h"
#include "simppl/detail/parented.h"
#include "simppl/detail/serialization.h"
#include "simppl/detail/clientresponseholder.h"
#include "simppl/stubbase.h"


// forward decl
template<typename> struct InterfaceNamer;


struct ClientResponseBase
{
   virtual void eval(const CallState& state, const void* payload, size_t len) = 0;
   
protected:

   inline
   ~ClientResponseBase()
   {
      // NOOP
   }
};


// ---------------------------------------------------------------------------------


struct ClientSignalBase : Parented
{
   virtual void eval(const void* data, size_t len) = 0;
   
   inline
   ClientSignalBase(uint32_t id)
    : id_(id)
   {
      // NOOP
   }

   inline   
   uint32_t id() const   // make this part of a generic Identifiable baseclass
   {
      return id_;
   }
   
protected:
   
   inline
   ~ClientSignalBase()
   {
      // NOOP
   }
   
   uint32_t id_;   
};


template<typename... T>
struct ClientSignal : ClientSignalBase
{
   static_assert(isValidType<T...>::value, "invalid type in interface");
   
   typedef std::function<void(typename CallTraits<T>::param_type...)> function_type;
      
   inline
   ClientSignal(uint32_t id, std::vector<Parented*>& parent)
    : ClientSignalBase(id)
   {
      parent.push_back(this);
   }
      
   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      assert(!f_);
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
         Deserializer d(payload, length);
         GetCaller<T...>::type::template eval(d, f_);
      }
      else
         std::cerr << "No appropriate handler registered for signal " << id_ << " with payload size=" << length << std::endl;
   }
   
   function_type f_;
};


template<typename... T, typename FuncT>
inline
void operator>>(ClientSignal<T...>& sig, const FuncT& func)
{
   sig.handledBy(func);
}


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


template<typename VectorT>
Deserializer& operator>>(Deserializer& istream, ClientVectorAttributeUpdate<VectorT>& updt)
{
   istream >> (uint32_t&)updt.how_;
   istream >> updt.where_;
   istream >> updt.len_;
   
   if (updt.how_ != Remove)
   {
      updt.data_.resize(updt.len_);
      
      for(int i=0; i<updt.len_; ++i)
      {
         istream >> updt.data_[i];
      }
   }
   
   return istream;
}


template<typename VectorT>
struct isValidType<ClientVectorAttributeUpdate<VectorT>>
{
   enum { value = true };
};


// -----------------------------------------------------------------------------------


template<typename DataT, typename EmitPolicyT>
struct ClientAttribute
{
   static_assert(isValidType<DataT>::value, "invalid type in interface");
   
   typedef typename CallTraits<DataT>::param_type arg_type;
   
   typedef typename if_<is_vector<DataT>::value, 
      std::function<void(arg_type, How, uint32_t, uint32_t)>, 
      std::function<void(arg_type)> >::type function_type;
   
   inline
   ClientAttribute(uint32_t id, std::vector<Parented*>& parent)
    : signal_(id, parent)
    , data_()
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
      assert(!f_);
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
   
private:

   /// vector argument with partial update support   
   typedef typename if_<is_vector<DataT>::value, ClientVectorAttributeUpdate<DataT>, DataT>::type signal_arg_type;
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
      setAndCall(arg);
   }
      
   signal_type signal_;
   DataT data_;
   
   function_type f_;
};


template<typename DataT, typename EmitPolicyT, typename FuncT>
inline
void operator>>(ClientAttribute<DataT,EmitPolicyT>& attr, const FuncT& func)
{
   attr.handledBy(func);
}


// --------------------------------------------------------------------------------


template<typename... T>
struct ClientRequest : Parented
{
   static_assert(isValidType<T...>::value, "invalid_type_in_interface");
      
   inline
   ClientRequest(uint32_t id, std::vector<Parented*>& parent)
    : id_(id)
    , handler_(0)
   {
      parent.push_back(this);
   }
      
   inline
   ClientResponseHolder operator()(typename CallTraits<T>::param_type... t)
   {
      Serializer s; //FIXME (sizeof(typename remove_ref<T1>::type));
      return ClientResponseHolder(handler_, parent<StubBase>()->sendRequest(*this, handler_, id_, serialize(s, t...)));
   }

   ClientResponseBase* handler_;
   uint32_t id_;
};


// ----------------------------------------------------------------------------------------


template<typename... T>
struct ClientResponse : ClientResponseBase
{ 
   static_assert(isValidType<T...>::value, "invalid_type_in_interface");
   
   typedef std::function<void(const CallState&, typename CallTraits<T>::param_type...)> function_type;
   
   inline
   ClientResponse()
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
   
   void eval(const CallState& cs, const void* payload, size_t length)
   {
      if (f_)
      {
         Deserializer d(payload, length);
         GetCaller<T...>::type::template evalResponse(d, f_, cs);
      }
      else
         std::cerr << "No appropriate handler registered for response with payload size=" << length << std::endl;
   }
   
   function_type f_;
};



template<typename FunctorT, typename... T>
inline 
ClientResponse<T...>& operator>> (ClientResponse<T...>& r, const FunctorT& f)
{
   r.handledBy(f);
   return r;
}


#include "simppl/stub.h"

#endif   // SIMPPL_CLIENTSIDE_H
