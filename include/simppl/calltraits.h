#ifndef CALLTRAITS_H
#define CALLTRAITS_H


#include "if.h"
#include "typetraits.h"


// FIXME references of T should stay references here!!!
// FIXME pointers constness is still a question?!
template<typename T>
struct CallTraits
{
   typedef T value_type;
   typedef const T const_value_type;
   typedef typename if_<is_reference<T>::value, T, typename remove_ref<T>::type&>::type reference_type;
   typedef typename if_<is_reference<T>::value, const T, const typename remove_ref<T>::type&>::type const_reference_type;   

   /**
    * This typedef could be used for optimized method parameter type dispatching.
    */
   typedef typename if_<is_integral<T>::value 
		               || is_float<T>::value 
		               || is_pointer<T>::value
                     || is_reference<T>::value, value_type, const_reference_type>::type param_type;

   typedef typename if_<is_reference<T>::value, value_type, reference_type>::type return_type;

   // FIXME remove_xxx is bullshit here
   typedef typename if_<is_pointer<T>::value, typename remove_ptr<typename remove_ref<T>::type>::type const*, typename if_<is_reference<T>::value, const_value_type, const_reference_type>::type>::type const_return_type;
};


#endif   // CALLTRAITS_H
