#ifndef SIMPPL_SERVERSIDE_H
#define SIMPPL_SERVERSIDE_H


#include <set>
#include <iostream>

#include "simppl/calltraits.h"
#include "simppl/callstate.h"
#include "simppl/attribute.h"

#include "simppl/detail/serverresponseholder.h"
#include "simppl/detail/serversignalbase.h"
#include "simppl/detail/validation.h"


struct ServerRequestBase
{
   friend struct ServerRequestBaseSetter;
   
   virtual void eval(const void* payload, size_t length) = 0;

   inline
   bool hasResponse() const
   {
      return hasResponse_;
   }
   
protected:

   inline 
   ServerRequestBase()
    : hasResponse_(false)
   {
      // NOOP
   }
   
   inline
   ~ServerRequestBase() 
   {
      // NOOP
   }
   
   bool hasResponse_;
};


// move to namespace detail
struct ServerRequestBaseSetter
{
   template<typename T>
   static inline
   void setHasResponse(T& t)
   {
      t.hasResponse_ = true;
   }
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
struct ServerSignal : ServerSignalBase
{
   static_assert(isValidType<T...>::value, "invalid type in interface");
   
   inline
   ServerSignal(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
   {
      _signals[id] = this;
   }
   
   inline
   void emit(typename CallTraits<T>::param_type... args)
   {
      Serializer s;
      sendSignal(serialize(s, args...));
   }
   
   
protected:
   
   inline
   void emitWithId(uint32_t registrationid, T... t)
   {
      Serializer s;
      serialize(s, t...);

      SignalSender(s.data(), s.size())(ServerSignalBase::recipients_[registrationid]);
   }
};


// -------------------------------------------------------------------------------------


template<typename... T>
struct ServerRequest : ServerRequestBase
{
   static_assert(isValidType<T...>::value, "invalid_type_in_interface");
   
   typedef std::function<void(typename CallTraits<T>::param_type...)> function_type;
     
   inline
   ServerRequest(uint32_t id, std::map<uint32_t, ServerRequestBase*>& requests)
   {
      requests[id] = this;
   }
   
   template<typename FunctorT>
   inline
   void handledBy(FunctorT func)
   {
      assert(!f_);
      f_ = func;
   }
      
   void eval(const void* payload, size_t length)
   {
      if (f_)
      {
         Deserializer d(payload, length);
         GetCaller<T...>::type::template eval(d, f_);
      }
      else
         std::cerr << "No appropriate handler registered for request with payload size=" << length << std::endl;
   }
   
   function_type f_;
};


template<typename FunctorT, typename... T>
inline 
void operator>> (ServerRequest<T...>& r, const FunctorT& f)
{
   r.handledBy(f);
}


// ---------------------------------------------------------------------------------------------


template<typename... T>
struct ServerResponse : ServerResponseBase
{   
   static_assert(isValidType<T...>::value, "invalid_type_in_interface");
   
   inline
   ServerResponse()
   {
      // NOOP
   }
   
   inline
   ServerResponseHolder operator()(typename CallTraits<T>::param_type&... t)
   { 
      Serializer s;
      return ServerResponseHolder(serialize(s, t...), *this);
   }
};


// ----------------------------------------------------------------------------------------


/// FIXME this class supports only move semantics!
struct ServerRequestDescriptor
{
   inline
   ServerRequestDescriptor()
    : requestor_(0)
    , fd_(-1)
    , sequence_nr_(INVALID_SEQUENCE_NR)
    , sessionid_(INVALID_SESSION_ID)
   {
      // NOOP
   }
   
   inline
   ServerRequestDescriptor(const ServerRequestDescriptor& rhs)
    : requestor_(rhs.requestor_)
    , fd_(rhs.fd_)
    , sequence_nr_(rhs.sequence_nr_)
    , sessionid_(rhs.sessionid_)
   {
      const_cast<ServerRequestDescriptor&>(rhs).clear();
   }
   
   ServerRequestDescriptor& operator=(const ServerRequestDescriptor& rhs)
   {
      if (this != &rhs)
      {
         requestor_ = rhs.requestor_;
         fd_ = rhs.fd_;
         sequence_nr_ = rhs.sequence_nr_;
         sessionid_ = rhs.sessionid_;
         
         const_cast<ServerRequestDescriptor&>(rhs).clear();
      }
      
      return *this;
   }
   
   ServerRequestDescriptor& set(ServerRequestBase* requestor, int fd, uint32_t sequence_nr, uint32_t sessionid)
   {
      requestor_ = requestor;
      fd_ = fd;
      sequence_nr_ = sequence_nr;
      sessionid_ = sessionid;
      return *this;
   }
   
   inline
   void clear()
   {
      set(0, -1, INVALID_SEQUENCE_NR, INVALID_SESSION_ID);
   }
   
   inline
   operator const void*() const
   {
      return requestor_;
   }
   
   ServerRequestBase* requestor_;
   int fd_;
   uint32_t sequence_nr_;
   uint32_t sessionid_;
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


template<typename VectorT>
Serializer& operator<<(Serializer& ostream, const ServerVectorAttributeUpdate<VectorT>& updt)
{
   ostream << (uint32_t)updt.how_;
   ostream << updt.where_;
   ostream << updt.len_;
   
   if (updt.how_ != Remove)
   {
      for(int i=updt.where_; i<updt.where_+updt.len_; ++i)
      {
         ostream << updt.data_[i];
      }
   }
   
   return ostream;
}


template<typename VectorT>
struct isValidType<ServerVectorAttributeUpdate<VectorT>>
{
   enum { value = true };
};


// --------------------------------------------------------------------------------------------


template<typename DataT>
struct BaseAttribute 
 : ServerSignal<typename if_<is_vector<DataT>::value, ServerVectorAttributeUpdate<DataT>, DataT>::type> 
{
   inline
   BaseAttribute(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
    : ServerSignal<typename if_<is_vector<DataT>::value, ServerVectorAttributeUpdate<DataT>, DataT>::type>(id, _signals)
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
__gnu_cxx::__normal_iterator<typename remove_const<IteratorT>::type, ContainerT>
unconst(__gnu_cxx::__normal_iterator<IteratorT, ContainerT>& iter)
{
   return __gnu_cxx::__normal_iterator<typename remove_const<IteratorT>::type, ContainerT>(
      const_cast<typename remove_const<IteratorT>::type>(iter.base()));
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
   VectorAttributeMixin(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
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
      emit(ServerVectorAttributeUpdate<T>(this->t_, Replace, this->t_.size()-1, 1));
   }
   
   inline
   void pop_back()
   {
      this->t_.pop_back();
      emit(ServerVectorAttributeUpdate<T>(this->t_, Remove, this->t_.size(), 1));
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
   CommitMixin(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
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
   CommitMixin(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
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
   : CommitMixin<EmitPolicyT, typename if_<is_vector<DataT>::value, VectorAttributeMixin<DataT, EmitPolicyT>, BaseAttribute<DataT> >::type> 
{
   static_assert(isValidType<DataT>::value, "invalid type in interface");
   
   typedef CommitMixin<EmitPolicyT, typename if_<is_vector<DataT>::value, VectorAttributeMixin<DataT, EmitPolicyT>, BaseAttribute<DataT> >::type>  baseclass;
   
   inline
   ServerAttribute(uint32_t id, std::map<uint32_t, ServerSignalBase*>& _signals)
    : baseclass(id, _signals)
   {
      // NOOP
   }
   
   inline
   ServerAttribute& operator=(const DataT& data)
   {
      this->t_ = data;
      
      if (EmitPolicyT::eval(this->t_, data))
         emit(this->t_);
      
      return *this;
   }   
   
   void onAttach(uint32_t registrationid)
   {
      emitWithId(registrationid, this->t_);
   }
};


#endif   // SIMPPL_SERVERSIDE_H