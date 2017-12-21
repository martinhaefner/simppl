#ifndef SIMPPL_FOR_EACH_H
#define SIMPPL_FOR_EACH_H


#include "typelist.h"


namespace simppl
{

template<typename ListT>
struct ForEach 
{
};


template<typename HeadT, typename TailT>
struct ForEach<TypeList<HeadT, TailT>>
{
   template<template<typename> class FuncT, typename ArgT>
   static inline
   void eval(ArgT& arg)
   {
      FuncT<HeadT>::eval(arg);
      ForEach<TailT>::template eval<FuncT>(arg);
   }
};


template<typename HeadT>
struct ForEach<TypeList<HeadT, NilType>>
{
   template<template<typename> class FuncT, typename ArgT>
   static inline
   void eval(ArgT& arg)
   {
      FuncT<HeadT>::eval(arg);
   }
};


template<>
struct ForEach<TypeList<NilType, NilType>>
{
   template<template<typename> class FuncT, typename ArgT>
   static inline
   void eval(ArgT& /*arg*/)
   {
      // NOOP
   }
};


template<size_t N>
struct StdTupleForEach
{
   template<typename TupleT, typename FunctorT>
   static inline
   void eval(TupleT& t, FunctorT func)
   {
     enum { __M = std::tuple_size<typename std::remove_const<TupleT>::type>::value - N };
      func(std::get<__M>(t));
      StdTupleForEach<N-1>::template eval(t, func);
   }
};

template<>
struct StdTupleForEach<1>
{
   template<typename TupleT, typename FunctorT>
   static inline
   void eval(TupleT& t, FunctorT func)
   {
     enum { __M = std::tuple_size<typename std::remove_const<TupleT>::type>::value - 1 };
      func(std::get<__M>(t));
   }
};


template<typename TupleT, typename FunctorT>
inline
void std_tuple_for_each(TupleT& t, FunctorT functor)
{
   StdTupleForEach<std::tuple_size<typename std::remove_const<TupleT>::type>::value>::template eval(t, functor);
}


}   // namespace simppl


#endif   // SIMPPL_FOR_EACH_H
