#ifndef SIMPPL_VARIANT_H
#define SIMPPL_VARIANT_H


#include "simppl/typelist.h"

#include <type_traits>


namespace simppl
{

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


template<typename... T>
struct Variant
{
   // FIXME make sure not to be able to add the same type multiple times

   typedef typename make_typelist<T...>::type typelist_type;

   enum { size = Max<typelist_type, SizeFunc>::value };
   enum { alignment = Max<typelist_type, AlignFunc>::value };

   /*private*/ enum { unset = -1 };

   inline
   Variant()
    : idx_(unset)
   {
      // NOOP
   }

   template<typename _T>
   inline
   Variant(const _T& t)             // FIXME use calltraits here
    : idx_(Find<_T, typelist_type>::value)
   {
      static_assert(Size<typelist_type>::value >= 0, "variant with size 0 invalid");
      static_assert(Find<_T, typelist_type>::value >= 0, "given_type_is_not_element_of_variant_maybe_use_explicit_cast");
      ::new(&data_) _T(t);
   }


   // FIXME implement inplace factories and assignment operator
   
   Variant(const Variant& rhs);
   Variant& operator=(const Variant& rhs);
   
   // NO INLINE, TOO LONG
   template<typename _T>
   Variant& operator=(const _T& t)            // FIXME use calltraits here
   {
      static_assert(Find<_T, typelist_type>::value >= 0, "given_type_is_not_element_of_variant_maybe_use_explicit_cast");

      if (idx_ == Find<_T, typelist_type>::value)
      {
          *get<_T>() = t;
      }
      else
      {
         try_destroy();
         idx_ = Find<_T, typelist_type>::value;
         ::new(&data_) _T(t);
      }

      return *this;
   }

   template<typename _T>
   inline
   _T* const get()
   {
       static_assert(Find<_T, typelist_type>::value >= 0, "given_type_is_not_element_of_variant_maybe_use_explicit_cast");

      if (Find<_T, typelist_type>::value != idx_)
         return 0;

      return (_T*)(&data_);
   }

   template<typename _T>
   inline
   const _T* const get() const
   {
       static_assert(Find<_T, typelist_type>::value >= 0, "given_type_is_not_element_of_variant_maybe_use_explicit_cast");

       if (Find<_T, typelist_type>::value != idx_)
           return 0;

       return (_T*)(&data_);
   }

// private

   template<int N>
   static inline
   void variant_destroy(void* t)
   {
       typedef typename RelaxedTypeAt<N, typelist_type>::type _T;
       ((_T*)t)->~_T();
   }

   // NON INLINE - too long!!!
   // FIXME write with recursive function instead
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
typename VisitorT::return_type staticVisit(VisitorT& visitor, VariantT& variant)
{
   // FIXME subsitute switch with static function table
   // FIXME recursive iterate
   switch(variant.idx_)
   {
   case 0:
       return Callfunc<typename RelaxedTypeAt<0, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 1:
       return Callfunc<typename RelaxedTypeAt<1, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 2:
       return Callfunc<typename RelaxedTypeAt<2, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 3:
       return Callfunc<typename RelaxedTypeAt<3, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   default:
      //std::cerr << "Hey, ugly!" << std::endl;
      throw;
   }
}


// FIXME make visitor a first class object
template<typename VisitorT, typename VariantT>
typename VisitorT::return_type staticVisit(VisitorT& visitor, const VariantT& variant)
{
   // FIXME subsitute switch with static function table
   // FIXME recursive iterate
   switch(variant.idx_)
   {
   case 0:
       return Callfunc<typename RelaxedTypeAt<0, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 1:
       return Callfunc<typename RelaxedTypeAt<1, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 2:
       return Callfunc<typename RelaxedTypeAt<2, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 3:
       return Callfunc<typename RelaxedTypeAt<3, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   default:
      //std::cerr << "Hey, ugly!" << std::endl;
      throw;
   }
}


namespace detail
{
   template<typename VariantT>
   struct ConstructionVisitor : StaticVisitor<>
   {
      ConstructionVisitor(VariantT& v)
       : v_(v)
      {
         // NOOP
      }
      
      template<typename T>
      void operator()(const T& t)
      {
         ::new(&v_.data_) T(t);
      }
      
      VariantT& v_;
   };
   
   
   template<typename VariantT>
   struct AssignmentVisitor : StaticVisitor<>
   {
      AssignmentVisitor(VariantT& v)
       : v_(v)
      {
         // NOOP
      }
      
      template<typename T>
      void operator()(const T& t)
      {
         *v_.template get<T>() = t;
      }
      
      VariantT& v_;
   };
}


template<typename... T>
Variant<T...>::Variant(const Variant<T...>& rhs)
 : idx_(rhs.idx_)
{
   if (idx_ != unset)
   {
      detail::ConstructionVisitor<Variant<T...>> v(*this);
      staticVisit(v, rhs);
   }
}


template<typename... T>
Variant<T...>& Variant<T...>::operator=(const Variant<T...>& rhs)
{
   if (this != &rhs)
   {
      if (idx_ != rhs.idx_)
      {
         // need to call copy constructor
         try_destroy();
         
         idx_ = rhs.idx_;
         detail::ConstructionVisitor<Variant<T...>> v(*this);
         staticVisit(v, rhs);
      }
      else
      {   
         detail::AssignmentVisitor<Variant<T...>> v(*this);
         staticVisit(v, rhs);
      }
   }
   
   return *this;
}   

}   // namespace simppl


#endif  // SIMPPL_VARIANT_H
