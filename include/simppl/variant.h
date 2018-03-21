#ifndef SIMPPL_VARIANT_H
#define SIMPPL_VARIANT_H


#include "simppl/typelist.h"
#include "simppl/serialization.h"

#include <type_traits>
#include <cstring>
#include <cassert>


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

   static_assert(sizeof...(T) <= 7, "currently only 7 variant entries supported");

   /*private*/ enum { unset = -1 };

   inline
   Variant()
    : idx_(unset)
   {
      // NOOP
   }

   inline
   ~Variant()
   {
       try_destroy();
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


   // TODO implement inplace factories and assignment operator

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


   bool empty() const
   {
       return idx_ == unset;
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
            &variant_destroy<3>,
            &variant_destroy<4>,
            &variant_destroy<5>,
            &variant_destroy<6>,
            &variant_destroy<7>
            // append if necessary
       };
       if (idx_ >= 0 && idx_ < Size<typelist_type>::value)
           funcs[(int)idx_](&data_);
   }

   // with an ordinary union only simple data types could be stored in here
   typename std::aligned_storage<size, alignment>::type data_;
   int8_t idx_;
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
typename VisitorT::return_type static_visit(VisitorT& visitor, VariantT& variant)
{
   // FIXME recursive iterate
   switch(variant.idx_)
   {
   // FIXME case -1:
   case 0:
       return Callfunc<typename RelaxedTypeAt<0, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 1:
       return Callfunc<typename RelaxedTypeAt<1, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 2:
       return Callfunc<typename RelaxedTypeAt<2, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 3:
       return Callfunc<typename RelaxedTypeAt<3, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 4:
       return Callfunc<typename RelaxedTypeAt<4, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 5:
       return Callfunc<typename RelaxedTypeAt<5, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 6:
       return Callfunc<typename RelaxedTypeAt<6, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 7:
      return Callfunc<typename RelaxedTypeAt<7, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   default:
      //std::cerr << "Hey, ugly!" << std::endl;
      throw;
   }
}


// FIXME make visitor a first class object
template<typename VisitorT, typename VariantT>
typename VisitorT::return_type static_visit(VisitorT& visitor, const VariantT& variant)
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

   case 4:
       return Callfunc<typename RelaxedTypeAt<4, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 5:
       return Callfunc<typename RelaxedTypeAt<5, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 6:
       return Callfunc<typename RelaxedTypeAt<6, typename VariantT::typelist_type>::type>::eval(visitor, variant);

   case 7:
       return Callfunc<typename RelaxedTypeAt<7, typename VariantT::typelist_type>::type>::eval(visitor, variant);

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
      static_visit(v, rhs);
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
         static_visit(v, rhs);
      }
      else
      {
         detail::AssignmentVisitor<Variant<T...>> v(*this);
         static_visit(v, rhs);
      }
   }

   return *this;
}


namespace dbus
{

namespace detail
{


struct VariantSerializer : StaticVisitor<>
{
   inline
   VariantSerializer(DBusMessageIter& iter)
    : iter_(iter)
   {
       // NOOP
   }

   template<typename T>
   void operator()(const T& t);

   DBusMessageIter& iter_;
};


template<typename... T>
struct VariantDeserializer;

template<typename T1, typename... T>
struct VariantDeserializer<T1, T...>
{
   template<typename VariantT>
   static
   bool eval(DBusMessageIter& iter, VariantT& v, const char* sig)
   {
      std::ostringstream buf;
      Codec<T1>::make_type_signature(buf);

      if (!strcmp(buf.str().c_str(), sig))
      {
         v = T1();
         Codec<T1>::decode(iter, *v.template get<T1>());

         return true;
      }
      else
         return VariantDeserializer<T...>::eval(iter, v, sig);
   }
};


template<typename T>
struct VariantDeserializer<T>
{
   template<typename VariantT>
   static
   bool eval(DBusMessageIter& iter, VariantT& v, const char* sig)
   {
      std::ostringstream buf;
      Codec<T>::make_type_signature(buf);

      if (!strcmp(buf.str().c_str(), sig))
      {
         v = T();
         Codec<T>::decode(iter, *v.template get<T>());

         return true;
      }

      // stop recursion
      return false;
   }
};


template<typename... T>
bool try_deserialize(DBusMessageIter& iter, Variant<T...>& v, const char* sig);


}   // namespace detail


template<typename... T>
struct Codec<Variant<T...>>
{
   static
   void encode(DBusMessageIter& iter, const Variant<T...>& v)
   {
      detail::VariantSerializer vs(iter);
      static_visit(vs, const_cast<Variant<T...>&>(v));   // TODO need const visitor
   }


   static
   void decode(DBusMessageIter& orig, Variant<T...>& v)
   {
      DBusMessageIter iter;
      simppl_dbus_message_iter_recurse(&orig, &iter, DBUS_TYPE_VARIANT);

      std::unique_ptr<char, void(*)(void*)> sig(dbus_message_iter_get_signature(&iter), &dbus_free);

      if (!detail::try_deserialize(iter, v, sig.get()))
         assert(false);

      dbus_message_iter_next(&orig);
   }


   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return os << DBUS_TYPE_VARIANT_AS_STRING;
   }
};


template<typename... T>
bool detail::try_deserialize(DBusMessageIter& iter, Variant<T...>& v, const char* sig)
{
   return VariantDeserializer<T...>::eval(iter, v, sig);
}


template<typename T>
inline
void detail::VariantSerializer::operator()(const T& t)   // seems to be already a reference so no copy is done
{
    std::ostringstream buf;
    Codec<T>::make_type_signature(buf);

    DBusMessageIter iter;
    dbus_message_iter_open_container(&iter_, DBUS_TYPE_VARIANT, buf.str().c_str(), &iter);

    Codec<T>::encode(iter, t);

    dbus_message_iter_close_container(&iter_, &iter);
}


}   // namespace dbus

}   // namespace simppl


#endif  // SIMPPL_VARIANT_H
