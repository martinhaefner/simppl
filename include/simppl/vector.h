#ifndef __SIMPPL_DBUS_VECTOR_H__
#define __SIMPPL_DBUS_VECTOR_H__


#include <vector>


namespace simppl
{

namespace dbus
{

namespace detail
{
 
   
template<typename T>
struct Codec<std::vector<T>>
{
   static 
   void encode(Serializer& s, const std::vector<T>& v)
   {
      DBusMessageIter iter;
      std::ostringstream buf;
      make_type_signature<T>::eval(buf);

      dbus_message_iter_open_container(s.iter_, DBUS_TYPE_ARRAY, buf.str().c_str(), &iter);

      Serializer s1(&iter);
      for (auto& t : v) {
         Codec<T>::encode(s1, t);
      }

      dbus_message_iter_close_container(s.iter_, &iter);
   }
   
   
   static 
   void decode(Deserializer& s, std::vector<T>& v)
   {
      v.clear();

      DBusMessageIter iter;
      dbus_message_iter_recurse(s.iter_, &iter);

      Deserializer ds(&iter);

      while(dbus_message_iter_get_arg_type(&iter) != 0)
      {
         T t;
         Codec<T>::decode(ds, t);
         v.push_back(t);
      }

      // advance to next element
      dbus_message_iter_next(s.iter_);
   }
};


template<typename T>
struct make_type_signature<std::vector<T>>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return make_type_signature<T>::eval(os << DBUS_TYPE_ARRAY_AS_STRING);
   }
};

   
}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // __SIMPPL_DBUS_VECTOR_H__
