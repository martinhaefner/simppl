#ifndef SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H
#define SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H

#include <dbus/dbus.h>
#include <tuple>
#include "simppl/serialization.h"

namespace simppl
{

namespace dbus
{

namespace detail
{

template<typename ReturnT>
struct deserialize_and_return_from_iter
{
   static
   ReturnT eval(DBusMessageIter* iter)
   {
      ReturnT rc;

      decode(*iter, rc);

      return rc;
   }
};


template<typename ReturnT>
struct deserialize_and_return
{
   static
   ReturnT eval(DBusMessage* msg)
   {
      DBusMessageIter iter;
      dbus_message_iter_init(msg, &iter);

      return deserialize_and_return_from_iter<ReturnT>::eval(&iter);
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

      DBusMessageIter iter;
      dbus_message_iter_init(msg, &iter);

      Codec<return_type>::decode_flattened(iter, rc);

      return rc;
   }
};


}

}

}


#endif   // SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H
