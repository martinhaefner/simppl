#ifndef BIND_H
#define BIND_H

#include "function.h"
#include "tuple.h"


template<int position>
struct PlaceHolder
{
    enum { pos = position };
};

PlaceHolder<1> _1;
PlaceHolder<2> _2;
PlaceHolder<3> _3;
PlaceHolder<4> _4;
PlaceHolder<5> _5;
PlaceHolder<6> _6;


template<typename BoundT>
struct Argument
{
    // the value is ignored here
    template<typename TupleT>
    static
    BoundT& resolve(BoundT& p, TupleT&)
    {
        return p;
    }
};


template<int pos>
struct Argument<PlaceHolder<pos> >
{
    // the placeholder is subsituted by the given value
    template<typename TupleT>
    static
    typename TypeAt<pos-1, typename TupleT::typelist_type>::type resolve(PlaceHolder<pos>& h, TupleT& t)
    {
        return t.template get<pos-1>();
    }
};


template<typename FuncT, typename T1>
struct Binder1
{
   typedef Function<FuncT> functor_type;
   typedef typename functor_type::return_type return_type;
   
   typedef typename if_<is_same<T1, PlaceHolder<1> >::value, typename functor_type::arg1_type, NilType>::type arg1_type;           

    Binder1(FuncT func, T1 t1)
     : func_(func)
     , t1_(t1)
    {
        // NOOP
    }    
    
    return_type operator()(arg1_type a1)
    {
        Tuple<arg1_type> args(a1);
        return func_(Argument<T1>::resolve(t1_, args));
    }
    
    return_type operator()()
    {
        return func_(t1_);
    }
    
    // ------------------------------------------------    
    // pointer-to-object support
    // ------------------------------------------------    
    
    return_type operator()(typename functor_type::object_type* a1)
    {
        Tuple<typename functor_type::object_type*> args(a1);
        return func_(Argument<T1>::resolve(t1_, args));
    }

    Function<FuncT> func_;
    T1 t1_;
};


template<typename FuncT, typename T1, typename T2>
struct Binder2
{
   typedef Function<FuncT> functor_type;
   typedef typename functor_type::return_type return_type;
   
   typedef typename if_<is_same<T1, PlaceHolder<1> >::value, typename functor_type::arg1_type,
           typename if_<is_same<T2, PlaceHolder<1> >::value, typename functor_type::arg2_type, NilType>::type>::type arg1_type;
   typedef typename if_<is_same<T1, PlaceHolder<2> >::value, typename functor_type::arg1_type,
           typename if_<is_same<T2, PlaceHolder<2> >::value, typename functor_type::arg2_type, NilType>::type>::type arg2_type;

    Binder2(FuncT func, T1 t1, T2 t2)
     : func_(func)
     , t1_(t1)
     , t2_(t2)
    {
        // NOOP
    }

    return_type operator()(arg1_type a1, arg2_type a2)
    {
        Tuple<arg1_type, arg2_type> args(a1, a2);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args));
    }

    return_type operator()(arg1_type a1)
    {
        Tuple<arg1_type> args(a1);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args));
    }

    return_type operator()()
    {
        return func_(t1_, t2_);
    }
        
    // ------------------------------------------------    
    // pointer-to-object support
    // ------------------------------------------------        
    
    return_type operator()(typename functor_type::object_type* a1, arg2_type a2)
    {
        Tuple<typename functor_type::object_type*, arg2_type> args(a1, a2);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args));
    }

    return_type operator()(typename functor_type::object_type* a1)
    {
        Tuple<typename functor_type::object_type*> args(a1);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args));
    }
    

    functor_type func_;
    T1 t1_;
    T2 t2_;
};


template<typename FuncT, typename T1, typename T2, typename T3>
struct Binder3
{
    typedef Function<FuncT> functor_type;
    typedef typename functor_type::return_type return_type;

    typedef typename if_<is_same<T1, PlaceHolder<1> >::value, typename functor_type::arg1_type,
            typename if_<is_same<T2, PlaceHolder<1> >::value, typename functor_type::arg2_type,
            typename if_<is_same<T3, PlaceHolder<1> >::value, typename functor_type::arg3_type, NilType>::type>::type>::type arg1_type;
    typedef typename if_<is_same<T1, PlaceHolder<2> >::value, typename functor_type::arg1_type,
            typename if_<is_same<T2, PlaceHolder<2> >::value, typename functor_type::arg2_type,
            typename if_<is_same<T3, PlaceHolder<2> >::value, typename functor_type::arg3_type, NilType>::type>::type>::type arg2_type;
    typedef typename if_<is_same<T1, PlaceHolder<3> >::value, typename functor_type::arg1_type,
            typename if_<is_same<T2, PlaceHolder<3> >::value, typename functor_type::arg2_type,
            typename if_<is_same<T3, PlaceHolder<3> >::value, typename functor_type::arg3_type, NilType>::type>::type>::type arg3_type;

    Binder3(FuncT func, T1 t1, T2 t2, T3 t3)
    : func_(func)
    , t1_(t1)
    , t2_(t2)
    , t3_(t3)
    {
        // NOOP
    }

    return_type operator()(arg1_type a1, arg2_type a2, arg3_type a3)
    {
        Tuple<arg1_type, arg2_type, arg3_type> args(a1, a2, a3);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }

    // FIXME member function pointer must be used with reference only - no pointer-to-object possible like in function object
    return_type operator()(arg1_type a1, arg2_type a2)
    {
        Tuple<arg1_type, arg2_type> args(a1, a2);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }

    return_type operator()(arg1_type a1)
    {
        Tuple<arg1_type> args(a1);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }

    return_type operator()()
    {
        return func_(t1_, t2_, t3_);
    }
    
    // ------------------------------------------------    
    // pointer-to-object support
    // ------------------------------------------------        
    
    return_type operator()(typename functor_type::object_type* a1, arg2_type a2, arg3_type a3)
    {
        Tuple<typename functor_type::object_type*, arg2_type, arg3_type> args(a1, a2, a3);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }
    
    return_type operator()(typename functor_type::object_type* a1, arg2_type a2)
    {
        Tuple<typename functor_type::object_type*, arg2_type> args(a1, a2);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }

    return_type operator()(typename functor_type::object_type* a1)
    {
        Tuple<typename functor_type::object_type*> args(a1);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }

    functor_type func_;
    T1 t1_;
    T2 t2_;
    T3 t3_;
};


template<typename FuncT, typename T1, typename T2, typename T3, typename T4>
struct Binder4
{
    typedef Function<FuncT> functor_type;
    typedef typename functor_type::return_type return_type;
    
    typedef typename if_<is_same<T1, PlaceHolder<1> >::value, typename functor_type::arg1_type,
            typename if_<is_same<T2, PlaceHolder<1> >::value, typename functor_type::arg2_type,
            typename if_<is_same<T3, PlaceHolder<1> >::value, typename functor_type::arg3_type,
            typename if_<is_same<T4, PlaceHolder<1> >::value, typename functor_type::arg4_type, NilType>::type>::type>::type>::type arg1_type;
    typedef typename if_<is_same<T1, PlaceHolder<2> >::value, typename functor_type::arg1_type,
            typename if_<is_same<T2, PlaceHolder<2> >::value, typename functor_type::arg2_type,
            typename if_<is_same<T3, PlaceHolder<2> >::value, typename functor_type::arg3_type,
            typename if_<is_same<T4, PlaceHolder<2> >::value, typename functor_type::arg4_type, NilType>::type>::type>::type>::type arg2_type;
    typedef typename if_<is_same<T1, PlaceHolder<3> >::value, typename functor_type::arg1_type,
            typename if_<is_same<T2, PlaceHolder<3> >::value, typename functor_type::arg2_type,
            typename if_<is_same<T3, PlaceHolder<3> >::value, typename functor_type::arg3_type,
            typename if_<is_same<T4, PlaceHolder<3> >::value, typename functor_type::arg4_type, NilType>::type>::type>::type>::type arg3_type;
    typedef typename if_<is_same<T1, PlaceHolder<4> >::value, typename functor_type::arg1_type,
            typename if_<is_same<T2, PlaceHolder<4> >::value, typename functor_type::arg2_type,
            typename if_<is_same<T3, PlaceHolder<4> >::value, typename functor_type::arg3_type,
            typename if_<is_same<T4, PlaceHolder<4> >::value, typename functor_type::arg4_type, NilType>::type>::type>::type>::type arg4_type;


    Binder4(FuncT func, T1 t1, T2 t2, T3 t3, T4 t4)
    : func_(func)
    , t1_(t1)
    , t2_(t2)
    , t3_(t3)
    , t4_(t4)
    {
        // NOOP
    }

    return_type operator()(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4)
    {
        Tuple<arg1_type, arg2_type, arg3_type, arg4_type> args(a1, a2, a3, a4);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), 
                     Argument<T3>::resolve(t3_, args), Argument<T4>::resolve(t4_, args));
    }

    return_type operator()(arg1_type a1, arg2_type a2, arg3_type a3)
    {
        Tuple<arg1_type, arg2_type, arg3_type> args(a1, a2, a3);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }
    
    return_type operator()(arg1_type a1, arg2_type a2)
    {
        Tuple<arg1_type, arg2_type> args(a1, a2);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }

    return_type operator()(arg1_type a1)
    {
        Tuple<arg1_type> args(a1);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }

    return_type operator()()
    {
        return func_(t1_, t2_, t3_, t4_);
    }
    
    // ------------------------------------------------    
    // pointer-to-object support
    // ------------------------------------------------    
    
    return_type operator()(typename functor_type::object_type* a1, arg2_type a2, arg3_type a3, arg4_type a4)
    {
        Tuple<typename functor_type::object_type*, arg2_type, arg3_type, arg4_type> args(a1, a2, a3, a4);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), 
                     Argument<T3>::resolve(t3_, args), Argument<T4>::resolve(t4_, args));
    }

    return_type operator()(typename functor_type::object_type* a1, arg2_type a2, arg3_type a3)
    {
        Tuple<typename functor_type::object_type*, arg2_type, arg3_type> args(a1, a2, a3);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }
    
    return_type operator()(typename functor_type::object_type* a1, arg2_type a2)
    {
        Tuple<typename functor_type::object_type*, arg2_type> args(a1, a2);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }

    return_type operator()(typename functor_type::object_type* a1)
    {
        Tuple<typename functor_type::object_type*> args(a1);
        return func_(Argument<T1>::resolve(t1_, args), Argument<T2>::resolve(t2_, args), Argument<T3>::resolve(t3_, args));
    }    

    functor_type func_;
    T1 t1_;
    T2 t2_;
    T3 t3_;
    T4 t4_;
};



// --------------------------------------------------------------------------------------


template<typename FuncT, typename T1>
inline
Binder1<FuncT, T1>
bind(FuncT func, T1 t1)
{
   return Binder1<FuncT, T1>(func, t1);
}

template<typename FuncT, typename T1, typename T2>
inline
Binder2<FuncT, T1, T2>
bind(FuncT func, T1 t1, T2 t2)
{
   return Binder2<FuncT, T1, T2>(func, t1, t2);
}

template<typename FuncT, typename T1, typename T2, typename T3>
inline
Binder3<FuncT, T1, T2, T3>
bind(FuncT func, T1 t1, T2 t2, T3 t3)
{
    return Binder3<FuncT, T1, T2, T3>(func, t1, t2, t3);
}

template<typename FuncT, typename T1, typename T2, typename T3, typename T4>
inline
Binder4<FuncT, T1, T2, T3, T4>
bind(FuncT func, T1 t1, T2 t2, T3 t3, T4 t4)
{
    return Binder4<FuncT, T1, T2, T3, T4>(func, t1, t2, t3, t4);
}


#endif  // BIND_H
