#ifndef SIMPPL_DETAIL_CALLINTERFACE_H
#define SIMPPL_DETAIL_CALLINTERFACE_H


#include "simppl/noninstantiable.h"
#include "simppl/typelist.h"
#include "simppl/callstate.h"
#include "simppl/tuple.h"


namespace simppl
{

namespace dbus
{

namespace detail
{

// FIXME use calltraits or rvalue references here

template<int N, typename TupleT>
struct FunctionCaller
{
   template<typename FunctorT>
   static inline
   void eval(FunctorT& f, const TupleT& tuple)
   {
      FunctionCaller<N, TupleT>::template eval_intern<FunctorT>(f, tuple);
   }

   template<typename FunctorT, typename ErrorT>
   static inline
   void eval_cs(FunctorT& f, const TCallState<ErrorT>& cs, const TupleT& tuple)
   {
      FunctionCaller<N, TupleT>::template eval_intern_cs<FunctorT, ErrorT>(f, cs, tuple);
   }

   template<typename FunctorT, typename... T>
   static inline
   void eval_intern(FunctorT& f, const TupleT& tuple, const T&... t)
   {
     FunctionCaller<N+1 == std::tuple_size<TupleT>::value ? -1 : N+1, TupleT>::template eval_intern<FunctorT>(f, tuple, t..., std::get<N>(tuple));
   }

   template<typename FunctorT, typename ErrorT, typename... T>
   static inline
   void eval_intern_cs(FunctorT& f, const TCallState<ErrorT>& cs, const TupleT& tuple, const T&... t)
   {
      FunctionCaller<N+1 == std::tuple_size<TupleT>::value ? -1 : N+1, TupleT>::template eval_intern_cs<FunctorT, ErrorT>(f, cs, tuple, t..., std::get<N>(tuple));
   }
};

template<typename TupleT>
struct FunctionCaller<-1, TupleT>
{
   template<typename FunctorT, typename... T>
   static inline
   void eval_intern(FunctorT& f, const TupleT& /*tuple*/, const T&... t)
   {
      f(t...);
   }

   template<typename FunctorT, typename ErrorT, typename... T>
   static inline
   void eval_intern_cs(FunctorT& f, const TCallState<ErrorT>& cs, const TupleT& /*tuple*/, const T&... t)
   {
      f(cs, t...);
   }
};

template<>
struct FunctionCaller<0, std::tuple<>>
{
   template<typename FunctorT, typename ErrorT, typename... T>
   static inline
   void eval_cs(FunctorT& f, const TCallState<ErrorT>& cs, const std::tuple<>& /*tuple*/)
   {
      f(cs);
   }
};


template<typename T>
struct DeserializeAndCall : simppl::NonInstantiable
{
   template<typename FunctorT>
   static
   void eval(DBusMessageIter& iter, FunctorT& f)
   {
      std::tuple<T> tuple;
      Codec<std::tuple<T>>::decode_flattened(iter, tuple);

      FunctionCaller<0, std::tuple<T>>::template eval<FunctorT>(f, tuple);
   }

   template<typename FunctorT, typename ErrorT>
   static
   void evalResponse(DBusMessageIter& iter, FunctorT& f, const simppl::dbus::TCallState<ErrorT>& cs)
   {
      std::tuple<T> tuple;

      if (cs)
         Codec<std::tuple<T>>::decode_flattened(iter, tuple);

      FunctionCaller<0, std::tuple<T>>::template eval_cs<FunctorT, ErrorT>(f, cs, tuple);
   }
};


template<typename... T>
struct DeserializeAndCall<std::tuple<T...>> : simppl::NonInstantiable
{
   template<typename FunctorT>
   static inline
   void eval(DBusMessageIter& iter, FunctorT& f)
   {
      std::tuple<T...> tuple;
      Codec<std::tuple<T...>>::decode_flattened(iter, tuple);

      FunctionCaller<0, std::tuple<T...>>::template eval<FunctorT>(f, tuple);
   }

   template<typename FunctorT, typename ErrorT>
   static
   void evalResponse(DBusMessageIter& iter, FunctorT& f, const simppl::dbus::TCallState<ErrorT>& cs)
   {
      std::tuple<T...> tuple;

      if (cs)
         Codec<std::tuple<T...>>::decode_flattened(iter, tuple);

      FunctionCaller<0, std::tuple<T...>>::template eval_cs<FunctorT, ErrorT>(f, cs, tuple);
   }
};


struct DeserializeAndCall0 : simppl::NonInstantiable
{
   template<typename FunctorT>
   static inline
   void eval(DBusMessageIter& /*iter*/, FunctorT& f)
   {
      f();
   }

   template<typename FunctorT, typename ErrorT>
   static inline
   void evalResponse(DBusMessageIter& /*iter*/, FunctorT& f, const simppl::dbus::TCallState<ErrorT>& cs)
   {
      f(cs);
   }
};


template<typename... T>
struct GetCaller : simppl::NonInstantiable
{
   typedef DeserializeAndCall<T...> type;
};

template<>
struct GetCaller<> : simppl::NonInstantiable
{
   typedef DeserializeAndCall0 type;
};

template<>
struct GetCaller<void> : simppl::NonInstantiable
{
   typedef DeserializeAndCall0 type;
};


}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_CALLINTERFACE_H
