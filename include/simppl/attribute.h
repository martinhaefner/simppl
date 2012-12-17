#ifndef SIMPPL_ATTRIBUTE_H
#define SIMPPL_ATTRIBUTE_H


#include <vector>


// partial update mode for attributes
enum How
{
   None = -1,
   Full = 0,
   Replace,   ///< replace and/or append (depending on given index)
   Remove,
   Insert,
};


// --------------------------------------------------------------------------------------


template<typename T>
struct is_vector
{
   enum { value = false };
};


template<typename T>
struct is_vector<std::vector<T> >
{
   enum { value = true };
};


struct OnChange
{
   template<typename T>
   static inline 
   bool eval(T& lhs, const T& rhs)
   {
      return lhs != rhs;
   }
};


struct Always
{
   template<typename T>
   static inline 
   bool eval(const T&, const T&)
   {
      return true;
   }
};


struct Committed
{
   template<typename T>
   static inline 
   bool eval(const T&, const T&)
   {
      return false;
   }
};


#endif   // SIMPPL_ATTRIBUTE_H