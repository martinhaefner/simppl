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
      d >> rc;
      
      return rc; 
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
   
}

}

}


#endif   // __SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H__
