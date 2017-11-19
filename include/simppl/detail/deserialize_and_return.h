#ifndef __SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H__
#define __SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H__


#include "serialization.h"


namespace simppl
{
   
namespace dbus
{
   
namespace detail
{

template<typename ReturnT> 
struct deserialize_and_return
{
   static 
   ReturnT eval(DBusMessage* msg)
   {
      ReturnT rc;
      
      Deserializer d(msg);
      d.read(rc);
      
      return std::move(rc); 
   }
};


template<>
struct deserialize_and_return<void>
{
   static
   void eval(DBusMessage*)
   {
      // NOOP
   }
};


// FIXME problem when return type is a single tuple instead of a parameter list
// then the flattened return would be problematic!
template<typename... T> 
struct deserialize_and_return<std::tuple<T...>>
{
   typedef std::tuple<T...> return_type;
   static 
   return_type eval(DBusMessage* msg)
   {
      return_type rc;
      
      Deserializer d(msg);
      d.read_flattened(rc);
      
      return std::move(rc); 
   }
};
   
}

}

}


#endif   // __SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H__
