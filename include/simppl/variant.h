#ifndef SIMPPL_VARIANT_H
#define SIMPPL_VARIANT_H


#include "simppl/typelist.h"
#include "simppl/serialization.h"

#include <variant>
#include <type_traits>
#include <cstring>
#include <cassert>
#include <memory>


namespace simppl
{

namespace dbus
{

namespace detail
{


struct VariantSerializer
{
   inline
   VariantSerializer(Encoder& e)
    : e_(e)
   {
       // NOOP
   }

   template<typename T>
   void operator()(const T& t);

   Encoder& e_;
};


template<typename... T>
bool try_deserialize(Decoder& d, std::variant<T...>& v, const char* sig);


}   // namespace detail


template<typename... T>
struct Codec<std::variant<T...>> : composite_signature<signature_chars<DBUS_TYPE_VARIANT>>
{
   static
   void encode(Encoder& e, const std::variant<T...>& v)
   {
      detail::VariantSerializer vs(e);
      std::visit(vs, const_cast<std::variant<T...>&>(v));   // TODO need const visitor
   }


   static
   void decode(Decoder& d, std::variant<T...>& v)
   {
      Decoder value = d.recurse(DBUS_TYPE_VARIANT);

      std::unique_ptr<char, void(*)(void*)> sig(dbus_message_iter_get_signature(&value.native()), &dbus_free);

      // none of the alternatives matches the received type
      if (!detail::try_deserialize(value, v, sig.get()))
         throw DecoderError();

      d.next();
   }
};


template<typename... T>
bool detail::try_deserialize(Decoder& d, std::variant<T...>& v, const char* sig)
{
   // the first alternative with matching signature wins
   return ((!strcmp(signature_of<T>(), sig) && (detail::decode_one<T>(d, v.template emplace<T>()), true)) || ...);
}


template<typename T>
inline
void detail::VariantSerializer::operator()(const T& t)   // seems to be already a reference so no copy is done
{
    Encoder value = e_.open_container(DBUS_TYPE_VARIANT, signature_of<T>());

    detail::encode_one<T>(value, t);
}


}   // namespace dbus

}   // namespace simppl


#endif  // SIMPPL_VARIANT_H
