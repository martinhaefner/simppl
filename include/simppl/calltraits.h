#ifndef CALLTRAITS_H
#define CALLTRAITS_H


#include <type_traits>


namespace simppl
{

// FIXME references of T should stay references here!!!
// FIXME pointers constness is still a question?!
template<typename T>
struct CallTraits
{
   typedef T value_type;
   typedef const T const_value_type;
   typedef typename std::conditional<std::is_reference<T>::value, T, typename std::remove_reference<T>::type&>::type reference_type;
   typedef typename std::conditional<std::is_reference<T>::value, const T, const typename std::remove_reference<T>::type&>::type const_reference_type;

   /**
    * This typedef could be used for optimized method parameter type dispatching.
    */
   typedef typename std::conditional<std::is_integral<T>::value
                       || std::is_floating_point<T>::value
                       || std::is_pointer<T>::value
                     || std::is_reference<T>::value, value_type, const_reference_type>::type param_type;

   typedef typename std::conditional<std::is_reference<T>::value, value_type, reference_type>::type return_type;

   // FIXME remove_xxx is bullshit here
   typedef typename std::conditional<std::is_pointer<T>::value, typename std::remove_pointer<typename std::remove_reference<T>::type>::type const*,
      typename std::conditional<std::is_reference<T>::value, const_value_type, const_reference_type>::type>::type const_return_type;
};

}   // namespace simppl


#endif   // CALLTRAITS_H
