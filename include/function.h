#ifndef FUNCTION_H
#define FUNCTION_H


#include "typelist.h"


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
};

template<typename ReturnT, typename ObjT, typename T1>
struct FunctionTraits<ReturnT(ObjT::*)(T1) const>
{
    typedef ReturnT               return_type;
    typedef const ObjT                  object_type;
    typedef TypeList<const ObjT&, TypeList<T1, NilType> > param_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2)>
{
   typedef ReturnT               return_type;
   typedef ObjT                  object_type;
   typedef TypeList<ObjT&, TypeList<T1, TypeList<T2, NilType> > > param_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2) const>
{
    typedef ReturnT               return_type;
    typedef const ObjT                  object_type;
    typedef TypeList<const ObjT&, TypeList<T1, TypeList<T2, NilType> > > param_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3)>
{
   typedef ReturnT               return_type;
   typedef ObjT                  object_type;
   typedef TypeList<ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, NilType> > > > param_type;
};

template<typename ReturnT, typename ObjT, typename T1, typename T2, typename T3>
struct FunctionTraits<ReturnT(ObjT::*)(T1, T2, T3) const>
{
    typedef ReturnT               return_type;
    typedef const ObjT                  object_type;
    typedef TypeList<const ObjT&, TypeList<T1, TypeList<T2, TypeList<T3, NilType> > > > param_type;
};

// --------------------------------------------------------------------------------------

// member functions by pointer
struct MemberFunctionCaller
{
    template<typename FuncT, typename ReturnT, typename ObjT>    
    static inline 
    ReturnT eval(FuncT func, ObjT obj)
    {
        return (obj.*func)();
    }

    template<typename FuncT, typename ReturnT, typename ObjT, typename T1>
    static inline 
    ReturnT eval(FuncT func, ObjT obj, T1 t1)
    {
        return (obj.*func)(t1);
    }

    template<typename FuncT, typename ReturnT, typename ObjT, typename T1, typename T2>
    static inline 
    ReturnT eval(FuncT func, ObjT obj, T1 t1, T2 t2)
    {
        return (obj.*func)(t1, t2);
    }

    template<typename FuncT, typename ReturnT, typename ObjT, typename T1, typename T2, typename T3>
    static inline 
    ReturnT eval(FuncT func, ObjT obj, T1 t1, T2 t2, T3 t3)
    {
        return (obj.*func)(t1, t2, t3);
    }
    
    
    template<typename FuncT, typename ReturnT, typename ObjT>    
    static inline 
    ReturnT eval(FuncT func, ObjT* obj)
    {
        return (obj->*func)();
    }

    template<typename FuncT, typename ReturnT, typename ObjT, typename T1>
    static inline 
    ReturnT eval(FuncT func, ObjT* obj, T1 t1)
    {
        return (obj->*func)(t1);
    }

    template<typename FuncT, typename ReturnT, typename ObjT, typename T1, typename T2>
    static inline 
    ReturnT eval(FuncT func, ObjT* obj, T1 t1, T2 t2)
    {
        return (obj->*func)(t1, t2);
    }

    template<typename FuncT, typename ReturnT, typename ObjT, typename T1, typename T2, typename T3>
    static inline 
    ReturnT eval(FuncT func, ObjT* obj, T1 t1, T2 t2, T3 t3)
    {
        return (obj->*func)(t1, t2, t3);
    }
};

// ordinary functions - no member functions
struct OrdinaryFunctionCaller
{   
   template<typename FuncT, typename ReturnT>
   static inline 
   ReturnT eval(FuncT func)
   {
      return func();
   }
   
   template<typename FuncT, typename ReturnT, typename T1>
   static inline 
   ReturnT eval(FuncT func, T1 t1)
   {      
      return func(t1);
   }
   
   template<typename FuncT, typename ReturnT, typename T1, typename T2>
   static inline 
   ReturnT eval(FuncT func, T1 t1, T2 t2)
   {
      return func(t1, t2);
   }
   
   template<typename FuncT, typename ReturnT, typename T1, typename T2, typename T3>
   static inline 
   ReturnT eval(FuncT func, T1 t1, T2 t2, T3 t3)
   {
      return func(t1, t2, t3);
   }
      
   template<typename FuncT, typename ReturnT, typename T1, typename T2, typename T3, typename T4>
   static inline 
   ReturnT eval(FuncT func, T1 t1, T2 t2, T3 t3, T4 t4)
   {
      return func(t1, t2, t3, t4);
   }
};


struct Caller
{
   template<typename FuncT, typename ReturnT>
   static inline
   ReturnT eval(FuncT& f)
   {
      return f();
   }   
   
   template<typename FuncT, typename ReturnT, typename Arg1T>
   static inline
   ReturnT eval(FuncT& f, Arg1T a1)
   {
      return f(a1);
   }   
   
   template<typename FuncT, typename ReturnT, typename Arg1T, typename Arg2T>
   static inline
   ReturnT eval(FuncT& f, Arg1T a1, Arg2T a2)
   {
      return f(a1, a2);
   }   
   
   template<typename FuncT, typename ReturnT, typename Arg1T, typename Arg2T, typename Arg3T>
   static inline
   ReturnT eval(FuncT& f, Arg1T a1, Arg2T a2, Arg3T a3)
   {
      return f(a1, a2, a3);
   }   
   
   template<typename FuncT, typename ReturnT, typename Arg1T, typename Arg2T, typename Arg3T, typename Arg4T>
   static inline
   ReturnT eval(FuncT& f, Arg1T a1, Arg2T a2, Arg3T a3, Arg4T a4)
   {
      return f(a1, a2, a3, a4);
   }   
};


struct Noop
{
   template<typename FuncT, typename ReturnT>
   static inline
   ReturnT eval(FuncT&)
   {  
      return ReturnT();
   }   
   
   template<typename FuncT, typename ReturnT, typename Arg1T>
   static inline
   ReturnT eval(FuncT&, Arg1T)
   {
      return ReturnT();
   }   
   
   template<typename FuncT, typename ReturnT, typename Arg1T, typename Arg2T>
   static inline
   ReturnT eval(FuncT&, Arg1T, Arg2T)
   {      
      return ReturnT();
   }   
   
   template<typename FuncT, typename ReturnT, typename Arg1T, typename Arg2T, typename Arg3T>
   static inline
   ReturnT eval(FuncT&, Arg1T, Arg2T, Arg3T)
   {  
      return ReturnT();
   }   
   
   template<typename FuncT, typename ReturnT, typename Arg1T, typename Arg2T, typename Arg3T, typename Arg4T>
   static inline
   ReturnT eval(FuncT&, Arg1T, Arg2T, Arg3T, Arg4T)
   {  
      return ReturnT();
   }   
};


/**
 * Functor implementation detail base class.
 */
template<typename ReturnT, typename Arg1T = NilType, typename Arg2T = NilType, typename Arg3T = NilType, typename Arg4T = NilType>
struct FunctionImpl
{   
   virtual ~FunctionImpl() {}
   
   virtual ReturnT eval() = 0;
   virtual ReturnT eval(Arg1T a1) = 0;
   virtual ReturnT eval(Arg1T a1, Arg2T a2) = 0;
   virtual ReturnT eval(Arg1T a1, Arg2T a2, Arg3T a3) = 0;
   virtual ReturnT eval(Arg1T a1, Arg2T a2, Arg3T a3, Arg4T a4) = 0;
   
   virtual FunctionImpl* duplicate() = 0;
};


template<typename FunctorT, typename ReturnT, 
         typename Arg1T = NilType, typename Arg2T = NilType, 
         typename Arg3T = NilType, typename Arg4T = NilType>
struct FunctorFunctionImpl : FunctionImpl<ReturnT, Arg1T, Arg2T, Arg3T, Arg4T>
{
   FunctorFunctionImpl(const FunctorT& t)
    : func_(t)
   {
      // NOOP
   }
   
   virtual FunctionImpl<ReturnT, Arg1T, Arg2T, Arg3T, Arg4T>* duplicate()
   {
      return new FunctorFunctionImpl(*this);
   }
   
   ReturnT eval()
   {
      typedef typename if_<is_same<Arg1T, NilType>::value, Caller, Noop>::type caller_type;
      return caller_type::template eval<FunctorT, ReturnT>(func_);
   }
   
   ReturnT eval(Arg1T a1)
   {            
      typedef typename if_<is_same<Arg2T, NilType>::value && !is_same<Arg1T, NilType>::value, Caller, Noop>::type caller_type;
      return caller_type::template eval<FunctorT, ReturnT, Arg1T>(func_, a1);
   }
   
   ReturnT eval(Arg1T a1, Arg2T a2)
   {
      typedef typename if_<is_same<Arg3T, NilType>::value && !is_same<Arg2T, NilType>::value, Caller, Noop>::type caller_type;
      return caller_type::template eval<FunctorT, ReturnT, Arg1T, Arg2T>(func_, a1, a2);
   }
   
   ReturnT eval(Arg1T a1, Arg2T a2, Arg3T a3)
   {
      typedef typename if_<is_same<Arg4T, NilType>::value && !is_same<Arg3T, NilType>::value, Caller, Noop>::type caller_type;
      return caller_type::template eval<FunctorT, ReturnT, Arg1T, Arg2T, Arg3T>(func_, a1, a2, a3);
   }
   
   ReturnT eval(Arg1T a1, Arg2T a2, Arg3T a3, Arg4T a4)
   {
      typedef typename if_<!is_same<Arg4T, NilType>::value, Caller, Noop>::type caller_type;
      return caller_type::template eval<FunctorT, ReturnT, Arg1T, Arg2T, Arg3T, Arg4T>(func_, a1, a2, a3, a4);
   }
   
   FunctorT func_;
};


template<typename T>
struct FunctionFunctionImpl : FunctionImpl<typename FunctionTraits<T>::return_type, 
                                           typename RelaxedTypeAt<0, typename FunctionTraits<T>::param_type>::type, 
                                           typename RelaxedTypeAt<1, typename FunctionTraits<T>::param_type>::type, 
                                           typename RelaxedTypeAt<2, typename FunctionTraits<T>::param_type>::type, 
                                           typename RelaxedTypeAt<3, typename FunctionTraits<T>::param_type>::type>
{
   typedef T function_type;
   typedef typename FunctionTraits<T>::return_type    return_type;
   typedef typename FunctionTraits<T>::object_type    object_type;   // NilType for non member functions
   
   typedef typename RelaxedTypeAt<0, typename FunctionTraits<T>::param_type>::type  arg1_type;  // for memfuns this is equal to object_type
   typedef typename RelaxedTypeAt<1, typename FunctionTraits<T>::param_type>::type  arg2_type;
   typedef typename RelaxedTypeAt<2, typename FunctionTraits<T>::param_type>::type  arg3_type;
   typedef typename RelaxedTypeAt<3, typename FunctionTraits<T>::param_type>::type  arg4_type;
   
private:
   
   typedef typename if_<is_same<object_type, NilType>::value, 
                        OrdinaryFunctionCaller, MemberFunctionCaller>::type caller_type;

public:      
   
   FunctionFunctionImpl(T t)
    : func_(t)
   {
      // NOOP
   }
   
   FunctionImpl<return_type, arg1_type, arg2_type, arg3_type, arg4_type>* duplicate()
   {
      return new FunctionFunctionImpl(*this);
   }
   
   return_type eval()
   {      
      typedef typename if_<is_same<arg1_type, NilType>::value, caller_type, Noop>::type real_caller_type;
      return real_caller_type::template eval<function_type, return_type>(func_);
   }
   
   return_type eval(arg1_type a1)
   {            
      typedef typename if_<is_same<arg2_type, NilType>::value && !is_same<arg1_type, NilType>::value, caller_type, Noop>::type real_caller_type;
      return real_caller_type::template eval<function_type, return_type, arg1_type>(func_, a1);
   }
   
   return_type eval(arg1_type a1, arg2_type a2)
   {
      typedef typename if_<is_same<arg3_type, NilType>::value && !is_same<arg2_type, NilType>::value, caller_type, Noop>::type real_caller_type;
      return real_caller_type::template eval<function_type, return_type, arg1_type, arg2_type>(func_, a1, a2);
   }
   
   return_type eval(arg1_type a1, arg2_type a2, arg3_type a3)
   {
      typedef typename if_<is_same<arg4_type, NilType>::value && !is_same<arg3_type, NilType>::value, caller_type, Noop>::type real_caller_type;
      return real_caller_type::template eval<function_type, return_type, arg1_type, arg2_type, arg3_type>(func_, a1, a2, a3);
   }
   
   return_type eval(arg1_type a1, arg2_type a2, arg3_type a3, arg4_type a4)
   {
      typedef typename if_<!is_same<arg4_type, NilType>::value, caller_type, Noop>::type real_caller_type;
      return real_caller_type::template eval<function_type, return_type, arg1_type, arg2_type, arg3_type, arg4_type>(func_, a1, a2, a3, a4);
   }
   
   T func_;
};


/**
 * Generic functor for binding functions and member functions. In the latter case, you may not use pointers to objects, but only 
 * references for now.
 */
template<typename T>
struct Function 
{
   typedef T function_type;
   
   typedef typename FunctionTraits<T>::return_type    return_type;
   
   typedef typename FunctionTraits<T>::object_type    object_type;   // NilType for non member functions   
   typedef typename FunctionTraits<T>::param_type     param_type;    // TypeList...
   
   typedef typename RelaxedTypeAt<0, typename FunctionTraits<T>::param_type>::type  arg1_type;  // for memfuns this is equal to object_type
   typedef typename RelaxedTypeAt<1, typename FunctionTraits<T>::param_type>::type  arg2_type;
   typedef typename RelaxedTypeAt<2, typename FunctionTraits<T>::param_type>::type  arg3_type;
   typedef typename RelaxedTypeAt<3, typename FunctionTraits<T>::param_type>::type  arg4_type;
         
         
   inline
   Function()       
    : p_(0)
   {  
      // NOOP
   }
   
   inline
   Function(T t)       
   {  
      p_ = new FunctionFunctionImpl<T>(t);
   }
      
   template<typename FunctorT>
   inline
   Function(FunctorT t)              
   {              
      p_ = new FunctorFunctionImpl<FunctorT, return_type, arg1_type, arg2_type, arg3_type, arg4_type>(t);
   }
   
   ~Function()
   {      
      delete p_;    
   }
   
   inline
   Function(const Function& f)       
   {  
      if (f.p_)
         p_ = f.p_->duplicate();      
   }
   
   inline
   Function& operator= (const Function& f)   
   {
      if (&f != this)
      {                  
         delete p_;
         
         if (f.p_)
            p_ = f.p_->duplicate();         
      }
      
      return *this;
   }
   
   inline
   Function& operator= (T t)   
   {         
      delete p_;
      p_ = new FunctionFunctionImpl<T>(t);   
      
      return *this;
   }
   
   template<typename FunctorT>
   inline
   Function& operator= (FunctorT t)            
   {              
      delete p_;      
      p_ = new FunctorFunctionImpl<FunctorT, return_type, arg1_type, arg2_type, arg3_type, arg4_type>(t);                  
      
      return *this;
   }
   
   inline
   operator const void* () const
   {
      return !!*this;
   }
   
   inline
   bool operator!() const
   {
      return p_ == 0;
   }
   
   // normal functions and reference to object
   inline
   return_type operator()()
   {         
      return p_->eval();      
   }

   inline
   return_type operator()(arg1_type t1)
   {          
      return p_->eval(t1);   
   }

   inline   
   return_type operator()(arg1_type t1, arg2_type t2)
   {         
      return p_->eval(t1, t2);      
   }

   inline
   return_type operator()(arg1_type t1, arg2_type t2, arg3_type t3)
   {         
      return p_->eval(t1, t2, t3);      
   }

   // @note the arguments are just the same as given in the function object -> programming bullshit there will yield bullshit argument
   //       types here...
   inline
   return_type operator()(arg1_type t1, arg2_type t2, arg3_type t3, arg4_type t4)
   {           
      return p_->eval(t1, t2, t3, t4);   
   }
   
   // ------------------------------------------------    
   // pointer-to-object support
   // ------------------------------------------------    
    
   inline
   return_type operator()(object_type* t1)
   {           
      return p_->eval(*t1);   
   }

   inline
   return_type operator()(object_type* t1, arg2_type t2)
   {         
      return p_->eval(*t1, t2);      
   }

   inline
   return_type operator()(object_type* t1, arg2_type t2, arg3_type t3)
   {         
      return p_->eval(*t1, t2, t3);      
   }
   
   inline
   return_type operator()(object_type* t1, arg2_type t2, arg3_type t3, arg4_type t4)
   {           
      return p_->eval(*t1, t2, t3, t4);   
   }

   FunctionImpl<return_type, arg1_type, arg2_type, arg3_type, arg4_type>* p_;   
};


#endif  // FUNCTION_H
