#ifndef SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H
#define SIMPPL_DETAIL_DESERIALIZE_AND_RETURN_H


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
      
      DBusMessageIter iter;
      dbus_message_iter_init(msg, &iter);
      
      decode(iter, rc);
      
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
