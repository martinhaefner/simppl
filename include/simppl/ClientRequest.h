#include <iostream>
#include <type_traits>
#include <tuple>
#include <functional>

#include "simppl/if.h"
#include "simppl/typelist.h"


using namespace simppl;



struct Oneway 
{
   typedef Oneway real_type;
};
 

// marker
template<typename T>
struct in
{
   typedef T real_type;
};

// marker
template<typename T>
struct out
{
   typedef T real_type;
};

// ---------------------------------------------------------------------

template<typename T>
struct is_in
{
   enum { value = false };
};

template<typename T>
struct is_in<in<T>>
{
   enum { value = true };
};

template<typename T>
struct is_out
{
   enum { value = false };
};

template<typename T>
struct is_out<out<T>>
{
   enum { value = true };
};

// ---------------------------------------------------------------------
 
// flatten typelist into std::tuple
template<typename ListT, typename TupleT>
struct make_tuple_from_list;

template<typename HeadT, typename TailT, typename... T>
struct make_tuple_from_list<TypeList<HeadT, TailT>, std::tuple<T...>>
{
   typedef typename make_tuple_from_list<TailT, std::tuple<T..., HeadT>>::type type;
};

template<typename HeadT, typename... T>
struct make_tuple_from_list<TypeList<HeadT, NilType>, std::tuple<T...>>
{
   typedef std::tuple<T..., HeadT> type;
};

template<typename... T>
struct make_tuple_from_list<TypeList<NilType, NilType>, std::tuple<T...>>
{
   typedef std::tuple<T...> type;
};

template<typename... T>
struct make_tuple_from_list<void, std::tuple<T...>>
{
   typedef std::tuple<T...> type;
};

// ---------------------------------------------------------------------
 
// flatten typelist into std::function
template<typename ListT, typename FunctT>
struct make_function_from_list;

template<typename HeadT, typename TailT, typename... T>
struct make_function_from_list<TypeList<HeadT, TailT>, std::function<void(int, T...)>>
{
   typedef typename make_function_from_list<TailT, std::function<void(int, T..., HeadT)>>::type type;
};

template<typename HeadT, typename... T>
struct make_function_from_list<TypeList<HeadT, NilType>, std::function<void(int, T...)>>
{
   typedef std::function<void(int, T..., HeadT)> type;
};

template<typename... T>
struct make_function_from_list<TypeList<NilType, NilType>, std::function<void(int, T...)>>
{
   typedef std::function<void(int, T...)> type;
};

template<typename... T>
struct make_function_from_list<void, std::function<void(int, T...)>>
{
   typedef std::function<void(int, T...)> type;
};

// ---------------------------------------------------------------------

// remove tuple<> if just one type is contained
template<typename... T>
struct canonify;

template<typename T>
struct canonify<T>
{
   typedef T type;
};

template<typename... T>
struct canonify<std::tuple<T...>>
{
   typedef std::tuple<T...> type;
};

template<typename T>
struct canonify<std::tuple<T>>
{
   typedef T type;
};

template<>
struct canonify<std::tuple<>>
{
   typedef void type;
};

// ---------------------------------------------------------------------

template<template<typename> class FilterFunc, typename... T> 
struct filter;

template<template<typename> class FilterFunc, typename T1, typename... T> 
struct filter<FilterFunc, T1, T...>
{   
   typedef typename std::conditional<FilterFunc<T1>::value, typename T1::real_type, NilType>::type first_type;
   typedef typename PushFront<typename filter<FilterFunc, T...>::list_type, first_type>::type list_type;
};

template<template<typename> class FilterFunc, typename T> 
struct filter<FilterFunc, T>
{   
   typedef typename std::conditional<FilterFunc<T>::value, typename T::real_type, NilType>::type first_type;
   typedef typename make_typelist<first_type>::type list_type;
};

template<template<typename> class FilterFunc> 
struct filter<FilterFunc>
{   
   typedef void list_type;
};

// ---------------------------------------------------------------------

template<typename... T> 
struct generate_return_type
{
   typedef typename filter<is_out, T...>::list_type list_type;
   typedef typename make_tuple_from_list<list_type, std::tuple<>>::type type;
};

template<typename... T> 
struct generate_argument_type
{
   typedef typename filter<is_in, T...>::list_type list_type;
   typedef typename make_tuple_from_list<list_type, std::tuple<>>::type type;
};

template<typename... T>
struct generate_callback_function
{
   typedef typename filter<is_out, T...>::list_type list_type;
   typedef typename make_function_from_list<list_type, std::function<void(int)>>::type type;
};

// ---------------------------------------------------------------------

template<typename T>
struct gen_dummy_return_value
{
   static inline 
   T eval()
   {
      return T();
   }
};

template<>
struct gen_dummy_return_value<void>
{
   static inline 
   void eval()
   {
      // NOOP
   }
};


template<typename... ArgsT>
struct is_oneway_request
{
   enum { value = Find<Oneway, typename make_typelist<ArgsT...>::type>::value != -1 };
};


// ---------------------------------------------------------------------


template<typename... ArgsT>
struct ClientRequest
{
   typedef typename canonify<typename generate_return_type<ArgsT...>::type>::type return_type;
   typedef typename canonify<typename generate_argument_type<ArgsT...>::type>::type args_type;
   typedef typename generate_callback_function<ArgsT...>::type callback_type;
   
   enum { is_oneway = is_oneway_request<ArgsT...>::value }; 
   static_assert(!is_oneway || is_oneway && std::is_same<return_type, void>::value, "oneway check");
   
   // blocking call
   template<typename... T>
   return_type operator()(const T&... args)
   {
      static_assert(std::is_same<typename canonify<std::tuple<typename std::decay<T>::type...>>::type, 
                    args_type>::value, "args mismatch");
      
      if (is_oneway)
      {
         return gen_dummy_return_value<return_type>::eval();
      }
      else
      
         // dummy return
         return gen_dummy_return_value<return_type>::eval();
   }
   
   // make switchable for oneway/non-oneway requests!
   struct Async
   {
      // async call
      template<typename... T>
      void operator()(const T&... args)
      {
         static_assert(is_oneway == false, "it's a oneway function");
         static_assert(std::is_same<typename canonify<std::tuple<typename std::decay<T>::type...>>::type, 
                       args_type>::value, "args mismatch");
      }
      
      // nonblocking asynchronous call needs a callback function
      callback_type f_;
      
   } async;
};


template<typename... ArgsT>
struct ServerRequest
{
   typedef typename canonify<typename generate_return_type<ArgsT...>::type>::type return_type;
   typedef typename canonify<typename generate_argument_type<ArgsT...>::type>::type args_type;
   typedef typename generate_callback_function<ArgsT...>::type callback_type;
   
   static_assert(!is_oneway || is_oneway && std::is_same<return_type, void>::value, "oneway check");
   
   template<typename... T>
   return_type operator()(const T&... args)
   {
      static_assert(std::is_same<typename canonify<std::tuple<typename std::decay<T>::type...>>::type, 
                    args_type>::value, "args mismatch");
      
      if (is_oneway)
      {
         return gen_dummy_return_value<return_type>::eval();
      }
      else
      
         // dummy return
         return gen_dummy_return_value<return_type>::eval();
   }
};

