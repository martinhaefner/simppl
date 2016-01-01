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

#include "simppl/detail/serverresponseholder.h"
#include "simppl/detail/basicinterface.h"
#include "simppl/detail/serversignalbase.h"
#include "simppl/detail/validation.h"

#if !(defined(SIMPPL_SKELETONBASE_H) \
      || defined(SIMPPL_DISPATCHER_CPP) \
      || defined(SIMPPL_SERVERSIGNALBASE_CPP) \
      || defined(SIMPPL_SKELETONBASE_CPP) \
      )
#   error "Don't include this file manually. Include 'skeleton.h' instead".
#endif


namespace simppl
{
   
namespace ipc
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
struct ServerSignal : detail::ServerSignalBase
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
      SkeletonBase* skel = dynamic_cast<SkeletonBase*>(parent_);
      
      //detail::Serializer s;
      //sendSignal(serialize(s, args...));
      DBusMessage* msg = dbus_message_new_signal(skel->role(), skel->iface(), name_);
   
      dbus_connection_send(parent_->conn_, msg, nullptr);
   }
   
   
protected:
   
   inline
   void emitWithId(uint32_t registrationid, T... t)
   {
      detail::Serializer s;
      serialize(s, t...);
    
      detail::SignalSender(s.data(), s.size())(detail::ServerSignalBase::recipients_[registrationid]);
   }
   
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
         //detail::Deserializer d(payload, length);
         //detail::GetCaller<T...>::type::template eval(d, f_);
         std::cout << "Request called" << std::endl;
         f_(42);
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
   ServerResponse(detail::BasicInterface* iface)
    : iface_(iface)
   {
      // NOOP
   }
   
   inline
   detail::ServerResponseHolder operator()(typename CallTraits<T>::param_type... t)
   { 
      SkeletonBase* skel = dynamic_cast<SkeletonBase*>(iface_);
      
      std::cout << "preparing response" << std::endl;
      DBusMessage* response = dbus_message_new_method_return(skel->currentRequest().msg_);
      // FIXME serialize
      
      DBusMessageIter args;
      dbus_message_iter_init_append(response, &args);
      int i = 42;
      dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &i);
      
      return detail::ServerResponseHolder(response, *this);
   }
   
   detail::BasicInterface* iface_;
};


// -----------------------------------------------------------------------------------------


template<typename VectorT>
struct ServerVectorAttributeUpdate
{
   ServerVectorAttributeUpdate(const VectorT& vec, How how = Full, uint32_t from = 0, uint32_t len = 0)
    : data_(vec)
    , how_(how)
    , where_(from)
    , len_(how == Full ? vec.size() : len)
   {
      // NOOP
   }
   
   const VectorT& data_;
   How how_;
   uint32_t where_;
   uint32_t len_;
};


namespace detail
{
   
template<typename VectorT>
struct isValidType<ServerVectorAttributeUpdate<VectorT>>
{
   enum { value = true };
};

}   // namespace detail


// --------------------------------------------------------------------------------------------


template<typename DataT>
struct BaseAttribute 
 : ServerSignal<typename if_<detail::is_vector<DataT>::value, ServerVectorAttributeUpdate<DataT>, DataT>::type> 
{
   inline
   BaseAttribute(uint32_t id, std::map<uint32_t, detail::ServerSignalBase*>& _signals)
    : ServerSignal<typename if_<detail::is_vector<DataT>::value, ServerVectorAttributeUpdate<DataT>, DataT>::type>(id, _signals)
   {
      // NOOP
   }
    
   inline
   const DataT& value() const
   {
      return t_;
   }
 
protected:
      
   DataT t_;
};


// horrible hack to remove the constness of the stl iterator for vector,
// maybe there are better solutions to achieve this?
template<typename IteratorT, typename ContainerT>
inline
__gnu_cxx::__normal_iterator<typename std::remove_const<IteratorT>::type, ContainerT>
unconst(__gnu_cxx::__normal_iterator<IteratorT, ContainerT>& iter)
{
   return __gnu_cxx::__normal_iterator<typename std::remove_const<IteratorT>::type, ContainerT>(
      const_cast<typename std::remove_const<IteratorT>::type>(iter.base()));
}


// forward decl
template<typename T, typename EmitPolicyT>
struct VectorAttributeMixin;


template<typename T, typename EmitPolicyT>
struct VectorValue
{
   inline
   VectorValue(VectorAttributeMixin<std::vector<T>, EmitPolicyT>& mixin, typename std::vector<T>::iterator iter)
    : mixin_(mixin)
    , iter_(iter)
   {
      // NOOP
   }
   
   T& operator=(const T& t);
   
   VectorAttributeMixin<std::vector<T>, EmitPolicyT>& mixin_;
   typename std::vector<T>::iterator iter_;
};
   

template<typename T, typename EmitPolicyT>
struct VectorAttributeMixin : BaseAttribute<T>
{
public:
   
   inline
   VectorAttributeMixin(uint32_t id, std::map<uint32_t, detail::ServerSignalBase*>& _signals)
    : BaseAttribute<T>(id, _signals)
   {
      // NOOP
   }
   
   typedef typename T::pointer pointer;
   typedef typename T::iterator iterator;
   typedef typename T::const_iterator const_iterator;
   typedef typename T::value_type value_type;
   
   template<typename IteratorT>
   inline
   void insert(const_iterator where, IteratorT from, IteratorT to)
   {
      insert(unconst(where), from, to, bool_<std::is_integral<IteratorT>::value>());
   }
   
   inline
   void insert(const_iterator where, const value_type& value)
   {
      this->t_.insert(unconst(where), value);
      emit(ServerVectorAttributeUpdate<T>(this->t_, Insert, where - this->t_.begin(), 1));
   }
   
   inline
   void erase(const_iterator where)
   {
      if (EmitPolicyT::eval(where, this->t_.end()))
      {
         this->t_.erase(iterator(const_cast<pointer>(where.base())));
         emit(ServerVectorAttributeUpdate<T>(this->t_, Remove, where-this->t_.begin(), 1));
      }
   }
   
   inline
   void erase(const_iterator from, const_iterator to)
   {
      if (EmitPolicyT::eval(from, this->t_.end()))
      {
         this->t_.erase(unconst(from), unconst(to));
         emit(ServerVectorAttributeUpdate<T>(this->t_, Remove, from-begin(), to-from));
      }
   }
   
   inline
   void clear()
   {
      erase(begin(), end());
   }
   
   inline
   bool empty() const
   {
      return this->t_.empty();
   }
   
   inline
   void push_back(const value_type& val)
   {
      this->t_.push_back(val);
      this->emit(ServerVectorAttributeUpdate<T>(this->t_, Replace, this->t_.size()-1, 1));
   }
   
   inline
   void pop_back()
   {
      this->t_.pop_back();
      this->emit(ServerVectorAttributeUpdate<T>(this->t_, Remove, this->t_.size(), 1));
   }
   
   inline
   size_t size() const
   {
      return this->t_.size();
   }
   
   inline
   const_iterator begin() const
   {
      return this->t_.begin();
   }
   
   inline
   const_iterator end() const
   {
      return this->t_.end();
   }
   
   inline
   VectorValue<value_type, EmitPolicyT> operator[](size_t idx)
   {
      return VectorValue<value_type, EmitPolicyT>(*this, this->t_.begin()+idx);
   }
   
   template<typename IteratorT>
   inline
   void replace(const_iterator where, IteratorT from, IteratorT to)
   {
      bool doEmit = false;
      
      size_t len = std::distance(from, to);
      for(iterator iter = unconst(where); iter != this->t_.end() && from != to; ++iter, ++from)
      {
         this->t_.replace(unconst(where), *from);
         
         if (EmitPolicyT::eval(*where, *from))
            doEmit = true;
      }

      if (from != to)
      {
         doEmit = true;

         while(from != to)
         {
            this->t_.push_back(*from);
            ++from;
         }
      }
      
      if (EmitPolicyT::eval(doEmit, false))
         emit(ServerVectorAttributeUpdate<T>(this->t_, Replace, where-this->t_.begin(), len));
   }
   
   inline
   void replace(const_iterator where, const value_type& v)
   {
      *unconst(where) = v;
         
      if (EmitPolicyT::eval(*where, v))
         emit(ServerVectorAttributeUpdate<T>(this->t_, Replace, where-this->t_.begin(), 1));
   }
   
protected:
   
   inline
   void insert(const_iterator where, size_t count, const value_type& value, tTrueType)
   {
      this->t_.insert(unconst(where), count, value);
      emit(ServerVectorAttributeUpdate<T>(this->t_, Insert, where - this->t_.begin(), count));
   }
   
   template<typename IteratorT>
   inline
   void insert(const_iterator where, IteratorT from, IteratorT to, tFalseType)
   {
      this->t_.insert(unconst(where), from, to);
      emit(ServerVectorAttributeUpdate<T>(this->t_, Insert, where - this->t_.begin(), std::distance(from, to)));
   }
};


template<typename T, typename EmitPolicyT>
inline
T& VectorValue<T, EmitPolicyT>::operator=(const T& t)
{
   mixin_.replace(iter_, t);
   return *iter_;
}


/// a NOOP
template<typename EmitPolicyT, typename BaseT>
struct CommitMixin : BaseT
{
   inline
   CommitMixin(uint32_t id, std::map<uint32_t, detail::ServerSignalBase*>& _signals)
    : BaseT(id, _signals)
   {
      // NOOP
   }
};


/// add a commit function to actively transfer the attribute data
template<typename BaseT>
struct CommitMixin<Committed, BaseT> : BaseT
{
   inline
   CommitMixin(uint32_t id, std::map<uint32_t, detail::ServerSignalBase*>& _signals)
    : BaseT(id, _signals)
   {
      // NOOP
   }
   
   void commit()
   {
      this->emit(this->t_);
   }
};


template<typename DataT, typename EmitPolicyT>
struct ServerAttribute 
   : CommitMixin<EmitPolicyT, typename if_<detail::is_vector<DataT>::value, VectorAttributeMixin<DataT, EmitPolicyT>, BaseAttribute<DataT> >::type> 
{
   static_assert(detail::isValidType<DataT>::value, "invalid type in interface");
   
   typedef CommitMixin<EmitPolicyT, typename if_<detail::is_vector<DataT>::value, VectorAttributeMixin<DataT, EmitPolicyT>, BaseAttribute<DataT> >::type>  baseclass;
   
   inline
   ServerAttribute(uint32_t id, std::map<uint32_t, detail::ServerSignalBase*>& _signals)
    : baseclass(id, _signals)
   {
      // NOOP
   }
   
   inline
   ServerAttribute& operator=(const DataT& data)
   {
      if (EmitPolicyT::eval(this->t_, data))
      {
         this->emit(data);
      }
      
      this->t_ = data;
      return *this;
   }   
   
   void onAttach(uint32_t registrationid)
   {
      this->emitWithId(registrationid, this->t_);
   }
};

}   // namespace ipc

}   // namespace simppl


template<typename FunctorT, typename... T>
inline 
void operator>>(simppl::ipc::ServerRequest<T...>& r, const FunctorT& f)
{
   r.handledBy(f);
}


template<typename VectorT>
simppl::ipc::detail::Serializer& operator<<(simppl::ipc::detail::Serializer& ostream, const simppl::ipc::ServerVectorAttributeUpdate<VectorT>& updt)
{
   ostream << (uint32_t)updt.how_;
   ostream << updt.where_;
   ostream << updt.len_;
   
   if (updt.how_ != simppl::ipc::Remove)
   {
      for(int i=updt.where_; i<updt.where_+updt.len_; ++i)
      {
         ostream << updt.data_[i];
      }
   }
   
   return ostream;
}


#endif   // SIMPPL_SERVERSIDE_H
