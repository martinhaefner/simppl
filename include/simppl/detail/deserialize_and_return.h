#ifndef SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H
#define SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H


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
   ReturnT eval(Decoder& d)
   {
      ReturnT rc;

      decode(d, rc);

      return rc;
   }
};


template<typename ReturnT>
struct deserialize_and_return
{
   static
   ReturnT eval(DBusMessage* msg)
   {
      Decoder d(msg);

      return deserialize_and_return_from_iter<ReturnT>::eval(d);
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

      Decoder d(msg);

      Codec<return_type>::decode_flattened(d, rc);

      return rc;
   }
};


}

}

}


#endif   // SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H
