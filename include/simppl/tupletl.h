#ifndef TUPLETL_H
#define TUPLETL_H

#include "typelist.h"
#include "calltraits.h"


template<typename T>
struct ValueHolder
{
    typedef T storage_type;
    typedef typename CallTraits<T>::param_type arg_type;

    ValueHolder()
     : head_()
    {
        // NOOP
    }

    ValueHolder(arg_type t)
     : head_(t)
    {
        // NOOP
    }

    T head_;
};

template<typename T1, typename T2>
struct ValueHolder<TypeList<T1, T2> > : public ValueHolder<T1>
{
    typedef typename ValueHolder<T1>::storage_type                                              storage_type;
    typedef typename CallTraits<T1>::param_type                                                 arg1_type;
    typedef typename CallTraits<typename RelaxedTypeAt<1, TypeList<T1, T2> >::type>::param_type arg2_type;
    typedef typename CallTraits<typename RelaxedTypeAt<2, TypeList<T1, T2> >::type>::param_type arg3_type;
    typedef typename CallTraits<typename RelaxedTypeAt<3, TypeList<T1, T2> >::type>::param_type arg4_type;

    ValueHolder()
     : ValueHolder<T1>()
     , tail_()
    {
        // NOOP
    }

    ValueHolder(arg1_type t1)
     : ValueHolder<T1>(t1)
     , tail_()
    {
        // NOOP
    }

    ValueHolder(arg1_type t1, arg2_type t2)
     : ValueHolder<T1>(t1)
     , tail_(t2)
    {
        // NOOP
    }

    ValueHolder(arg1_type t1, arg2_type t2, arg3_type t3)
     : ValueHolder<T1>(t1)
     , tail_(t2, t3)
    {
        // NOOP
    }

    ValueHolder(arg1_type t1, arg2_type t2, arg3_type t3, arg4_type t4)
     : ValueHolder<T1>(t1)
     , tail_(t2, t3, t4)
    {
        // NOOP
    }

    ValueHolder<T2> tail_;
};

// ---------------------------------------------------------------------------------------

template<int N, typename TupleT> struct Get;

template<int N, typename HeadT, typename TailT>
struct Get<N, ValueHolder<TypeList<HeadT, TailT> > >
{
    typedef typename CallTraits<typename TypeAt<N, TypeList<HeadT, TailT> >::type>::return_type       return_type;
    typedef typename CallTraits<typename TypeAt<N, TypeList<HeadT, TailT> >::type>::const_return_type const_return_type;

    static inline return_type value(ValueHolder<TypeList<HeadT, TailT> >& t)
    {
        return Get<N-1, ValueHolder<TailT> >::value(t.tail_);
    }
    static inline const_return_type value(const ValueHolder<TypeList<HeadT, TailT> >& t)
    {
        return Get<N-1, ValueHolder<TailT> >::value(t.tail_);
    }
};

template<typename HeadT, typename TailT>
struct Get<0, ValueHolder<TypeList<HeadT, TailT> > >
{
    typedef typename CallTraits<typename TypeAt<0, TypeList<HeadT, TailT> >::type>::return_type       return_type;
    typedef typename CallTraits<typename TypeAt<0, TypeList<HeadT, TailT> >::type>::const_return_type const_return_type;

    static inline return_type value(ValueHolder<TypeList<HeadT, TailT> >& t)
    {
        return t.head_;
    }
    static inline const_return_type value(const ValueHolder<TypeList<HeadT, TailT> >& t)
    {
        return t.head_;
    }
};

// -------------------------------------------------------------------------

template<typename TypeListT>
struct TupleTL : ValueHolder<TypeListT>
{  
public:

   typedef TypeListT typelist_type; 

protected:

   typedef ValueHolder<typelist_type> parent_type;

   typedef typename parent_type::arg1_type arg1_type;
   typedef typename parent_type::arg2_type arg2_type;
   typedef typename parent_type::arg3_type arg3_type;
   typedef typename parent_type::arg4_type arg4_type;
   
public:

    TupleTL()
     : parent_type()
    {
        // NOOP
    }

    TupleTL(arg1_type t1)
     : parent_type(t1)
    {
        // NOOP
    }

    TupleTL(arg1_type t1, arg2_type t2)
     : parent_type(t1, t2)
    {
        // NOOP
    }

    TupleTL(arg1_type t1, arg2_type t2, arg3_type t3)
     : parent_type(t1, t2, t3)
    {
        // NOOP
    }

    TupleTL(arg1_type t1, arg2_type t2, arg3_type t3, arg4_type t4)
     : parent_type(t1, t2, t3, t4)
    {
        // NOOP
    }

    template<int N>
    typename CallTraits<typename TypeAt<N, typelist_type>::type>::return_type get()
    {
        return Get<N, parent_type>::value(*this);
    }

    template<int N>
    typename CallTraits<typename TypeAt<N, typelist_type>::type>::const_return_type get() const
    {
        return Get<N, parent_type>::value(*this);
    }
};


// -----------------------------------------------------------------------------------------


// forward decl
template<typename T1, typename T2, typename T3, typename T4>
struct makeTL;

template<typename T1>
struct makeTL<T1, NilType, NilType, NilType>
{
   typedef TYPELIST_1(T1) type;
};

template<typename T1, typename T2>
struct makeTL<T1, T2,  NilType, NilType>
{
   typedef TYPELIST_2(T1, T2) type;
};

template<typename T1, typename T2, typename T3>
struct makeTL<T1, T2, T3, NilType>
{
   typedef TYPELIST_3(T1, T2, T3) type;
};

template<typename T1, typename T2, typename T3, typename T4>
struct makeTL
{
   typedef TYPELIST_4(T1, T2, T3, T4) type;
};


template<typename T1, typename T2 = NilType, typename T3 = NilType, typename T4 = NilType> 
class Tuple : TupleTL<typename makeTL<T1, T2, T3, T4>::type>
{
   typedef TupleTL<typename makeTL<T1, T2, T3, T4>::type> parent_type;

   typedef typename parent_type::arg1_type arg1_type;
   typedef typename parent_type::arg2_type arg2_type;
   typedef typename parent_type::arg3_type arg3_type;
   typedef typename parent_type::arg4_type arg4_type;

public:

   Tuple()
     : parent_type()
    {
        // NOOP
    }

    Tuple(arg1_type t1)
     : parent_type(t1)
    {
        // NOOP
    }

    Tuple(arg1_type t1, arg2_type t2)
     : parent_type(t1, t2)
    {
        // NOOP
    }

    Tuple(arg1_type t1, arg2_type t2, arg3_type t3)
     : parent_type(t1, t2, t3)
    {
        // NOOP
    }

    Tuple(arg1_type t1, arg2_type t2, arg3_type t3, arg4_type t4)
     : parent_type(t1, t2, t3, t4)
    {
        // NOOP
    }

   using parent_type::get;
};


#endif   // TUPLETL_H
