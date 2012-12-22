#ifndef SIMPPL_VARIANT_H
#define SIMPPL_VARIANT_H


#include "simppl/typelist.h"
   
#include <type_traits>


struct AlignFunc
{
    template<typename T>
    struct apply_
    {
        enum { value = std::alignment_of<T>::value };
    };
};


struct SizeFunc
{
    template<typename T>
    struct apply_
    {
        enum { value = sizeof(T) };
    };
};


// FIXME the algorithm should look a little bit like max_element and return an index of the
// maximum element as given by the Functor
template<typename ListT, typename FuncT>
struct Max;

template<typename HeadT, typename TailT, typename FuncT>
struct Max<TypeList<HeadT, TailT>, FuncT>
{
    typedef typename FuncT::template apply_<HeadT> first_type__;
    enum { value1__ = first_type__::value };
    enum { value2__ = Max<TailT, FuncT>::value };

    enum { value = (int)value1__ > (int)value2__ ? (int)value1__ : (int)value2__ };
};


template<typename HeadT, typename FuncT>
struct Max<TypeList<HeadT, NilType>, FuncT>
{
    typedef typename FuncT::template apply_<HeadT> type__;
    enum { value = type__::value };
};


// ----------------------------------------------------------------------------------------------


template<typename T1, typename T2, typename T3 = NilType, typename T4 = NilType>
struct Variant
{    
   // FIXME make sure not to be able to add the same type multiple times
   
    typedef TypeList<T1, TypeList<T2, NilType> > tl1__;
    typedef typename if_<std::is_same<T3, NilType>::value, tl1__, typename PushBack<tl1__, T3>::type>::type tl2__;
    typedef typename if_<std::is_same<T4, NilType>::value, tl2__, typename PushBack<tl2__, T4>::type>::type tl3__;

    typedef tl3__ typelist_type;

   enum { size = Max<typelist_type, SizeFunc>::value };
   enum { alignment = Max<typelist_type, AlignFunc>::value };
   
   /*private*/ enum { unset = -1 };   
      
   inline
   Variant()
    : idx_(unset)  
   {
      // NOOP
   }

   template<typename T>
   inline
   Variant(const T& t)             // FIXME use calltraits here
    : idx_(Find<T, typelist_type>::value)
   {
      static_assert(!std::is_same<T1, NilType>::value && !std::is_same<T2, NilType>::value, "a_variant_must_contain_at_least_two_elements");
      static_assert(Find<T, typelist_type>::value >= 0, "given_type_is_not_element_of_variant_maybe_use_explicit_cast");
      ::new(&data_) T(t);
   }

   // FIXME implement copy constructor, inplace factories and assignment operator

   // NO INLINE, TOO LONG
   template<typename T>
   Variant& operator=(const T& t)            // FIXME use calltraits here
   {
      static_assert(Find<T, typelist_type>::value >= 0, "given_type_is_not_element_of_variant_maybe_use_explicit_cast");

      if (idx_ == Find<T, typelist_type>::value)
      {
          *get<T>() = t;
      }
      else
      {
         try_destroy();
         idx_ = Find<T, typelist_type>::value;
         ::new(&data_) T(t);
      }

      return *this;
   }

   template<typename T>
   inline
   T* const get()
   {
       static_assert(Find<T, typelist_type>::value >= 0, "given_type_is_not_element_of_variant_maybe_use_explicit_cast");

      if (Find<T, typelist_type>::value != idx_)
         return 0;

      return (T*)(&data_);
   }

   template<typename T>
   inline
   const T* const get() const
   {
       static_assert(Find<T, typelist_type>::value >= 0, "given_type_is_not_element_of_variant_maybe_use_explicit_cast");

       if (Find<T, typelist_type>::value != idx_)
           return 0;

       return (T*)(&data_);
   }

// private

   template<int N>
   static inline
   void variant_destroy(void* t)
   {
       typedef typename RelaxedTypeAt<N, typelist_type>::type T;
       ((T*)t)->~T();
   }

   // NON INLINE - too long!!!
   void try_destroy()
   {      
      // Beware that the direction of types in the typelist is in reverse order!!!
       typedef void(*func_type)(void*);
       static func_type funcs[] = {
            &variant_destroy<0>,
            &variant_destroy<1>,
            &variant_destroy<2>,
            &variant_destroy<3>
            // append if necessary
       };
       if (idx_ >= 0 && idx_ < Size<typelist_type>::value)
           funcs[idx_](&data_);
   }  

   // with an ordinary union only simple data types could be stored in here
   typename std::aligned_storage<size, alignment>::type data_;
   char idx_;
};


template<typename ReturnT = void>
struct StaticVisitor
{
   typedef ReturnT return_type;
};


template<typename ParamT>
struct Callfunc
{
   template<typename VisitorT, typename VariantT>
   static inline
   typename VisitorT::return_type eval(VisitorT& visitor, VariantT& variant)
   {
      return visitor(*variant.template get<ParamT>());
   }
};

template<>
struct Callfunc<NilType>
{
   template<typename VisitorT, typename VariantT>
   static inline
   typename VisitorT::return_type eval(VisitorT&, VariantT&)
   {
      throw;
   }
};


template<typename VisitorT, typename VariantT>
typename VisitorT::return_type staticVisit(const VisitorT& visitor, VariantT& variant)
{
   VisitorT& the_visitor = const_cast<VisitorT&>(visitor);
   // FIXME subsitute switch with static function table
   switch(variant.idx_)
   {
   case 0:
       return Callfunc<typename RelaxedTypeAt<0, typename VariantT::typelist_type>::type>::eval(the_visitor, variant);

   case 1:
       return Callfunc<typename RelaxedTypeAt<1, typename VariantT::typelist_type>::type>::eval(the_visitor, variant);

   case 2:
       return Callfunc<typename RelaxedTypeAt<2, typename VariantT::typelist_type>::type>::eval(the_visitor, variant);

   case 3:
       return Callfunc<typename RelaxedTypeAt<3, typename VariantT::typelist_type>::type>::eval(the_visitor, variant);

   default:
      std::cerr << "Hey, ugly!" << std::endl;
      throw;
   }
}


#endif  // SIMPPL_VARIANT_H
