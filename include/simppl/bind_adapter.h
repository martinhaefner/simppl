#ifndef SIMPPL_BIND_ADAPTER_H
#define SIMPPL_BIND_ADAPTER_H


#include <functional>

#include "simppl/function_traits.h"
#include "simppl/if.h"

#define BIND_ std::bind
#define IS_PLACEHOLDER_ std::is_placeholder


namespace simppl
{

namespace detail
{
   
template<typename ReturnT, typename Arg1T, typename FunctorT>
struct f1_wrapper
{
   typedef ReturnT return_type;
   
   typedef Arg1T argument_type;   
   typedef Arg1T arg1_type;
   
   
   explicit inline
   f1_wrapper(const FunctorT& f)
    : f_(f)
   {
      // NOOP
   }
   
   inline
   ReturnT operator()(Arg1T arg)
   {
      return f_(arg);
   }
   
   inline
   ReturnT operator()()
   {
      return f_();
   }
   
   FunctorT f_;
};

   
/// deduce argument_type from placeholder position
template<typename FunctionT, typename Arg1T, typename Arg2T>
struct get_argument_type_2
{
   typedef typename if_<IS_PLACEHOLDER_<Arg1T>::value,
      typename TypeAt<0, typename FunctionTraits<FunctionT>::param_type>::type,
      typename TypeAt<1, typename FunctionTraits<FunctionT>::param_type>::type>::type type;
};


template<typename FunctionT, typename Arg1T, typename Arg2T, typename Arg3T>
struct get_argument_type_3
{
   typedef
      typename if_<IS_PLACEHOLDER_<Arg1T>::value,
         typename TypeAt<0, typename FunctionTraits<FunctionT>::param_type>::type,
         typename if_<IS_PLACEHOLDER_<Arg2T>::value,
            typename TypeAt<1, typename FunctionTraits<FunctionT>::param_type>::type,
            typename TypeAt<2, typename FunctionTraits<FunctionT>::param_type>::type>::type>::type
      type;
};

}   // namespace detail


// ordinary function forwarder - one argument function
template<typename FuncT, typename Arg1T>
inline
auto bind(FuncT func, Arg1T arg)
 -> decltype(detail::f1_wrapper<typename FunctionTraits<FuncT>::return_type, typename TypeAt<0, typename FunctionTraits<FuncT>::param_type>::type, decltype(BIND_(func, arg))>(BIND_(func, arg)))
{
   typedef typename FunctionTraits<FuncT>::return_type return_type;
   typedef typename TypeAt<0, typename FunctionTraits<FuncT>::param_type>::type arg_type;
   
   return detail::f1_wrapper<return_type, arg_type, decltype(BIND_(func, arg))>(BIND_(func, arg));
}


// binder for 2-argument functions (i.e. member functions with one argument - and 'this')
template<typename FunctionT, typename Arg1T, typename Arg2T>
inline
auto bind(FunctionT f, Arg1T arg1, Arg2T arg2)
 -> decltype(detail::f1_wrapper<
    typename FunctionTraits<FunctionT>::return_type,
    typename detail::get_argument_type_2<FunctionT, Arg1T, Arg2T>::type,
    decltype(BIND_(f, arg1, arg2))   
    >(BIND_(f, arg1, arg2)))
{
   typedef typename FunctionTraits<FunctionT>::return_type return_type;
   typedef typename detail::get_argument_type_2<FunctionT, Arg1T, Arg2T>::type argument_type;
   typedef decltype(BIND_(f, arg1, arg2)) functor_type;
  
   return detail::f1_wrapper<return_type, argument_type, functor_type>(BIND_(f, arg1, arg2));
}


// binder for 3-argument functions (i.e. member functions with two arguments - and 'this')
template<typename FunctionT, typename Arg1T, typename Arg2T, typename Arg3T>
inline
auto bind(FunctionT f, Arg1T arg1, Arg2T arg2, Arg3T arg3)
 -> decltype(detail::f1_wrapper<
       typename FunctionTraits<FunctionT>::return_type,
       typename detail::get_argument_type_3<FunctionT, Arg1T, Arg2T, Arg3T>::type,
       decltype(BIND_(f, arg1, arg2, arg3))   
       >(BIND_(f, arg1, arg2, arg3)))
{
   typedef typename FunctionTraits<FunctionT>::return_type return_type;
   typedef typename detail::get_argument_type_3<FunctionT, Arg1T, Arg2T, Arg3T>::type argument_type;
   typedef decltype(BIND_(f, arg1, arg2, arg3)) functor_type;
  
   return detail::f1_wrapper<return_type, argument_type, functor_type>(BIND_(f, arg1, arg2, arg3));
}

}   // namespace simppl


#endif   // SIMPPL_BIND_ADAPTER_H
