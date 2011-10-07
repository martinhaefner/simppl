#ifndef REF_H
#define REF_H


template<typename T>
struct SHBReferenceWrapper
{
   /**
    * Stored value type.
    */
   typedef T tType;   

   /**
    * This class is not intended for direct instantiation. See function @c hbref or @c hbcref instead.
    */
   explicit
   inline   
   SHBReferenceWrapper(T& t)
    : t_(t)
   {
      // NOOP
   }

   /**
    * Cast operator.
    *
    * @return a reference of the stored data type.
    */
   inline
   operator T&() const
   {
      return t_;
   }
   
   /**
    * Accessor function.
    *
    * @return a reference of the stored data type.
    */
   inline
   T& get() const
   {
      return t_;
   }
   
private:
   
   T& t_;
};

template<typename T>
inline
SHBReferenceWrapper<T> ref(T& t)
{
   return SHBReferenceWrapper<T>(t);
}


#endif   // REF_H
