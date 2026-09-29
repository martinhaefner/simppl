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

   
}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_VECTOR_H
