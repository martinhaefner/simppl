#ifndef SIMPPL_FUNCTION_TRAITS_H
#define SIMPPL_FUNCTION_TRAITS_H


#include "simppl/typelist.h"


// template forward declaration
template<typename T>
struct FunctionTraits;

template<typename ReturnT>
struct FunctionTraits<ReturnT(*)()>
{
   typedef ReturnT                    return_type;
   typedef NilType                    object_type;
   typedef TypeList<NilType, NilType> param_type;
};

template<typename ReturnT, typename T1>
struct FunctionTraits<ReturnT(*)(T1)>
{
   typedef ReturnT               return_type;
   typedef NilType               object_type;
   typedef TypeList<T1, NilType> param_type;
};

template<typename ReturnT, typename T1, typename T2>
struct FunctionTraits<ReturnT(*)(T1, T2)>
{
   typedef ReturnT                    return_type;
   typedef NilType                    object_type;
   typedef TypeList<T1, TypeList<T2, NilType> > param_type;
};

template<typename ReturnT, typename T1, typename T2, typename T3>
struct FunctionTraits<ReturnT(*)(T1, T2, T3)>
{
   typedef ReturnT                                              return_type;
   typedef NilType                    object_type;
   typedef TypeList<T1, TypeList<T2, TypeList<T3, NilType> > > param_type;
};

template<typename ReturnT, typename T1, typename T2, typename T3, typename T4>
struct FunctionTraits<ReturnT(*)(T1, T2, T3, T4)>
{
   typedef ReturnT                                                            return_type;
   typedef NilType                                                            object_type;
   typedef TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, NilType> > > > param_type;
};

// ---------------------------------------------------------------------------------------

template<typename ReturnT>
struct FunctionTraits<ReturnT()>
{
   typedef ReturnT                    return_type;
   typedef NilType                    object_type;
   typedef TypeList<NilType, NilType> param_type;
};

template<typename ReturnT, typename T1>
struct FunctionTraits<ReturnT(T1)>
{
   typedef ReturnT               return_type;
   typedef NilType               object_type;
   typedef TypeList<T1, NilType> param_type;

   typedef T1 arg1_type;
};

template<typename ReturnT, typename T1, typename T2>
struct FunctionTraits<ReturnT(T1, T2)>
{
   typedef ReturnT                    return_type;
   typedef NilType                    object_type;
   typedef TypeList<T1, TypeList<T2, NilType> > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
};

template<typename ReturnT, typename T1, typename T2, typename T3>
struct FunctionTraits<ReturnT(T1, T2, T3)>
{
   typedef ReturnT                                              return_type;
   typedef NilType                    object_type;
   typedef TypeList<T1, TypeList<T2, TypeList<T3, NilType> > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
};

template<typename ReturnT, typename T1, typename T2, typename T3, typename T4>
struct FunctionTraits<ReturnT(T1, T2, T3, T4)>
{
   typedef ReturnT                                                            return_type;
   typedef NilType                                                            object_type;
   typedef TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, NilType> > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
   typedef T4 arg4_type;
};

template<typename ReturnT, typename T1, typename T2, typename T3, typename T4, typename T5>
struct FunctionTraits<ReturnT(T1, T2, T3, T4, T5)>
{
   typedef ReturnT                                                            return_type;
   typedef NilType                                                            object_type;
   typedef TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, TypeList<T5, NilType> > > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
   typedef T4 arg4_type;
   typedef T5 arg5_type;
};

template<typename ReturnT, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct FunctionTraits<ReturnT(T1, T2, T3, T4, T5, T6)>
{
   typedef ReturnT                                                            return_type;
   typedef NilType                                                            object_type;
   typedef TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, TypeList<T5, TypeList<T6, NilType> > > > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
   typedef T4 arg4_type;
   typedef T5 arg5_type;
   typedef T6 arg6_type;
};

// ---------------------------------------------------------------------------------------

template<typename ReturnT, typename ObjT>
struct FunctionTraits<ReturnT(ObjT::*)()>
{
   typedef ReturnT                    return_type;
   typedef ObjT                       object_type;
   typedef TypeList<ObjT&, NilType>    param_type;
};

template<typename ReturnT, typename ObjT>
struct FunctionTraits<ReturnT(ObjT::*)() const>
{
    typedef ReturnT                    return_type;
    typedef const ObjT                       object_type;
    typedef TypeList<const ObjT&, NilType>    param_type;
};

template<typename ReturnT, typename ObjT, typename T1>
struct FunctionTraits<ReturnT(ObjT::*)(T1)>
{
   typedef ReturnT               return_type;
   typedef ObjT                  object_type;
   typedef TypeList<ObjT&, TypeList<T1, NilType> > param_type;

   typedef T1 arg1_type;
};

template<typename ReturnT, typename ObjT, typename T1>
struct FunctionTraits<ReturnT(ObjT::*)(T1) const>
{
   typedef ReturnT               return_type;
   typedef const ObjT                  object_type;
   typedef TypeList<const ObjT&, TypeList<T1, NilType> > param_type;

   typedef T1 arg1_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2)>
{
   typedef ReturnT               return_type;
   typedef ObjT                  object_type;
   typedef TypeList<ObjT&, TypeList<T1, TypeList<T2, NilType> > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2) const>
{
    typedef ReturnT               return_type;
    typedef const ObjT                  object_type;
    typedef TypeList<const ObjT&, TypeList<T1, TypeList<T2, NilType> > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;   
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3)>
{
   typedef ReturnT               return_type;
   typedef ObjT                  object_type;
   typedef TypeList<ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, NilType> > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3) const>
{
    typedef ReturnT               return_type;
    typedef const ObjT                  object_type;
    typedef TypeList<const ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, NilType> > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3, typename T4>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3, T4)>
{
   typedef ReturnT               return_type;
   typedef ObjT                  object_type;
   typedef TypeList<ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, NilType> > > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
   typedef T4 arg4_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3, typename T4>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3, T4) const>
{
    typedef ReturnT               return_type;
    typedef const ObjT                  object_type;
    typedef TypeList<const ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, NilType> > > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
   typedef T4 arg4_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3, typename T4, typename T5>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3, T4, T5)>
{
   typedef ReturnT               return_type;
   typedef ObjT                  object_type;
   typedef TypeList<ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, TypeList<T5, NilType> > > > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
   typedef T4 arg4_type;
   typedef T5 arg5_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3, typename T4, typename T5>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3, T4, T5) const>
{
    typedef ReturnT               return_type;
    typedef const ObjT                  object_type;
    typedef TypeList<const ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, TypeList<T5, NilType> > > > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
   typedef T4 arg4_type;
   typedef T5 arg5_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3, T4, T5, T6)>
{
   typedef ReturnT               return_type;
   typedef ObjT                  object_type;
   typedef TypeList<ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, TypeList<T5, TypeList<T6, NilType> > > > > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
   typedef T4 arg4_type;
   typedef T5 arg5_type;
   typedef T6 arg6_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3, T4, T5, T6) const>
{
    typedef ReturnT               return_type;
    typedef const ObjT                  object_type;
    typedef TypeList<const ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, TypeList<T4, TypeList<T5, TypeList<T6, NilType> > > > > > > param_type;

   typedef T1 arg1_type;
   typedef T2 arg2_type;
   typedef T3 arg3_type;
   typedef T4 arg4_type;
   typedef T5 arg5_type;
   typedef T6 arg6_type;
};


#endif   // SIMPPL_FUNCTION_TRAITS_H