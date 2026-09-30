#ifndef SIMPPL_DBUS_VECTOR_H
#define SIMPPL_DBUS_VECTOR_H


#include <vector>

#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{
    
   
template<typename T>
struct Codec<std::vector<T>> : composite_signature<signature_chars<DBUS_TYPE_ARRAY>, Codec<T>>
{
   static 
   void encode(Encoder& e, const std::vector<T>& v)
   {
      Encoder array = e.open_container(DBUS_TYPE_ARRAY, signature_of<T>());

      for (auto& t : v) 
      {
         detail::encode_one<T>(array, t);
      }
   }
   
   
   static 
   void decode(Decoder& d, std::vector<T>& v)
   {
      v.clear();

      Decoder array = d.recurse(DBUS_TYPE_ARRAY);

      while(!array.at_end())
      {
         T t;
         detail::decode_one<T>(array, t);
         v.push_back(t);
      }

      // advance to next element
      d.next();
   }
};



/**
 * std::vector<bool> packs its elements, so it is converted from and to
 * an array of dbus_bool_t in one go.
 */
template<>
struct Codec<std::vector<bool>> : composite_signature<signature_chars<DBUS_TYPE_ARRAY, DBUS_TYPE_BOOLEAN>>
{
   static
   void encode(Encoder& e, const std::vector<bool>& v)
   {
      const std::vector<dbus_bool_t> values(v.begin(), v.end());

      Encoder array = e.open_container(DBUS_TYPE_ARRAY, DBUS_TYPE_BOOLEAN_AS_STRING);
      array.append_fixed_array(DBUS_TYPE_BOOLEAN, values.data(), values.size());
   }


   static
   void decode(Decoder& d, std::vector<bool>& v)
   {
      Decoder array = d.recurse(DBUS_TYPE_ARRAY);

      const dbus_bool_t* values = nullptr;
      int n = 0;
      dbus_message_iter_get_fixed_array(&array.native(), &values, &n);

      v.assign(values, values + n);

      // advance to next element
      d.next();
   }
};

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_VECTOR_H
