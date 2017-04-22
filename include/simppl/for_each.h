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

}   // namespace simppl


#endif   // SIMPPL_FOR_EACH_H
