#ifndef TUPLE_H
#define TUPLE_H

#include "typelist.h"


template<typename T>
struct ValueHolder
{
    typedef T       storage_type;
    typedef NilType tail_type;
    
    ValueHolder()
     : head_()
    {
        // NOOP
    }

    ValueHolder(T t)
     : head_(t)
    {
        // NOOP
    }

    T head_;
};

template<typename T1, typename T2>
struct ValueHolder<TypeList<T1, T2> > : public ValueHolder<T1>
{
    typedef typename ValueHolder<T1>::storage_type             storage_type;
    typedef ValueHolder<T2>                                    tail_type;
    
    typedef T1                                                 arg1_type;
    typedef typename RelaxedTypeAt<1, TypeList<T1, T2> >::type arg2_type;
    typedef typename RelaxedTypeAt<2, TypeList<T1, T2> >::type arg3_type;
    typedef typename RelaxedTypeAt<3, TypeList<T1, T2> >::type arg4_type;

    ValueHolder()
     : ValueHolder<T1>()
     , tail_()
    {
        // NOOP
    }

    ValueHolder(T1 t1)
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

    tail_type tail_;
};

// ---------------------------------------------------------------------------------------

template<int N, typename TupleT> struct Get;

template<int N, typename HeadT, typename TailT>
struct Get<N, ValueHolder<TypeList<HeadT, TailT> > >
{
    typedef typename add_reference<typename TypeAt<N, TypeList<HeadT, TailT> >::type>::type       return_type;
    typedef typename add_reference<typename TypeAt<N, TypeList<HeadT, TailT> >::const_type>::type const_return_type;

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
    // FIXME return values that are already refs or pointers don't need to be referenced, but others must!
    typedef typename add_reference<typename ValueHolder<TypeList<HeadT, TailT> >::storage_type>::type return_type;
    typedef typename add_reference<const typename ValueHolder<TypeList<HeadT, TailT> >::storage_type>::type const_return_type;

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

// FIXME namespace detail...
template<typename T1, typename T2, typename T3, typename T4>
struct TupleHelper
{
   // FIXME add static check T1 must not be NilType!
   typedef TypeList<T1, NilType> t1__;
   typedef typename if_<is_same<T2, NilType>::value, t1__, typename PushBack<t1__, T2>::type>::type t2__;
   typedef typename if_<is_same<T3, NilType>::value, t2__, typename PushBack<t2__, T3>::type>::type t3__;
   typedef typename if_<is_same<T4, NilType>::value, t3__, typename PushBack<t3__, T4>::type>::type t4__;   
   typedef t4__ typelist_type;

    typedef ValueHolder<typelist_type> parent_type;
};


template<typename T1, typename T2 = NilType, typename T3 = NilType, typename T4 = NilType>
struct Tuple : TupleHelper<T1, T2, T3, T4>::parent_type
{   
private:
   
   typedef typename TupleHelper<T1, T2, T3, T4>::parent_type    parent_type;
   
public:

   typedef typename TupleHelper<T1, T2, T3, T4>::typelist_type  typelist_type;


    Tuple()
     : parent_type()
    {
        // NOOP
    }

    Tuple(T1 t1)
     : parent_type(t1)
    {
        // NOOP
    }

    Tuple(T1 t1, T2 t2)
     : parent_type(t1, t2)
    {
        // NOOP
    }

    Tuple(T1 t1, T2 t2, T3 t3)
     : parent_type(t1, t2, t3)
    {
        // NOOP
    }

    Tuple(T1 t1, T2 t2, T3 t3, T4 t4)
     : parent_type(t1, t2, t3, t4)
    {
        // NOOP
    }

    // FIXME use calltraits here...
    template<int N>
    typename add_reference<typename TypeAt<N, typelist_type>::type>::type get()
    {
        return Get<N, parent_type>::value(*this);
    }

    // FIXME handle intinsic datatypes as return by copy here!
    template<int N>
    typename add_reference<typename TypeAt<N, typelist_type>::const_type>::type get() const
    {
        return Get<N, parent_type>::value(*this);
    }
};


// ------------------------------------------------------------------------------------------------
// tuple algorithms - maybe they don't work...
// ------------------------------------------------------------------------------------------------


template<typename TupleT, typename FuncT, int idx=0> 
struct ForEach;

template<typename TupleT, typename FuncT, int idx> 
struct ForEach
{
    static inline 
    void eval(TupleT& t, FuncT f)
    {
        f(t.head_, idx);
        ForEach<typename TupleT::tail_type, FuncT, idx+1>::eval(t.tail_, f);
    }
    static inline 
    void eval(const TupleT& t, FuncT f)
    {
        f(t.head_, idx);
        ForEach<typename TupleT::tail_type, FuncT, idx+1>::eval(t.tail_, f);
    }
};

template<typename T, typename FuncT, int idx> 
struct ForEach<ValueHolder<TypeList<T, NilType> >, FuncT, idx>
{
    static inline 
    void eval(ValueHolder<TypeList<T, NilType> >& t, FuncT f) 
    { 
       f(t.head_, idx);
    }
    
    static inline 
    void eval(const ValueHolder<TypeList<T, NilType> >& t, FuncT f) 
    { 
       f(t.head_, idx);
    }
};


template<typename TupleT, typename FuncT> 
inline 
void for_each(TupleT& t, FuncT func)
{
    return ForEach<TupleT, FuncT>::eval(t, func);
}

template<typename TupleT, typename FuncT> 
inline 
void for_each(const TupleT& t, FuncT func)
{
    return ForEach<TupleT, FuncT>::eval(t, func);
}


// ---------------------------------------------------------------------------------

// does this work???
template<typename TupleT, typename FuncT, int idx=0> struct CountIf;

template<typename TupleT, typename FuncT, int idx> struct CountIf
{
    static inline int eval(TupleT& t, FuncT& f)
    {
        int ret = 0;

        if (f(t.head_, idx))
            ++ret;

        return ret + CountIf<typename TupleT::tail_type, FuncT, idx+1>::eval(t.tail_, f);
    }
    static inline int eval(const TupleT& t, FuncT& f)
    {
        int ret = 0;

        if (f(t.head_, idx))
            ++ret;

        return ret + CountIf<typename TupleT::tail_type, FuncT, idx+1>::eval(t.tail_, f);
    }
};

template<typename FuncT, int idx> struct CountIf<NilType, FuncT, idx>
{
    static inline int eval(NilType&, FuncT&) { return 0; }
    static inline int eval(const NilType&, FuncT&) { return 0; }
};


template<typename TupleT, typename FuncT> inline int count_if(TupleT& t, FuncT& func)
{
    return CountIf<TupleT, FuncT>::eval(t, func);
}

template<typename TupleT, typename FuncT> inline int count_if(const TupleT& t, FuncT& func)
{
    return CountIf<TupleT, FuncT>::eval(t, func);
}


#endif   // TUPLE_H
