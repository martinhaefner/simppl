#ifndef SIMPPL_ATTRIBUTE_H
#define SIMPPL_ATTRIBUTE_H


#include "simppl/detail/is_vector.h"


namespace simppl
{
   
namespace ipc
{

/// partial update mode for container attributes of type vector
enum How
{
   None = -1,   ///< no change at all
   Full = 0,    ///< full container is transferred
   Replace,     ///< replace and/or append (depending on given index)
   Remove,      ///< parts of the container are removed
   Insert       ///< some new elements are inserted
};


// --------------------------------------------------------------------------------------


/// attribute update signalling policy: only when attribute value changed on server
struct OnChange
{
   template<typename T>
   static inline 
   bool eval(T& lhs, const T& rhs)
   {
      return lhs != rhs;
   }
};


/// attribute update signalling policy: always send signal when operator= is called, even 
/// if the value remains the same.
struct Always
{
   template<typename T>
   static inline 
   bool eval(const T&, const T&)
   {
      return true;
   }
};


/// mostly good for updates on huge data structures, so not each assignment will fire
/// a signal but the signal is first fired when commit() is called.
struct Committed
{
   template<typename T>
   static inline 
   bool eval(const T&, const T&)
   {
      return false;
   }
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_ATTRIBUTE_H