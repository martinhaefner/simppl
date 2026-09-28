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
   VariantSerializer(DBusMessageIter& iter)
    : iter_(iter)
   {
       // NOOP
   }

   template<typename T>
   void operator()(const T& t);

   DBusMessageIter& iter_;
};


template<typename... T>
bool try_deserialize(DBusMessageIter& iter, std::variant<T...>& v, const char* sig);


}   // namespace detail


template<typename... T>
struct Codec<std::variant<T...>> : composite_signature<signature_chars<DBUS_TYPE_VARIANT>>
{
   static
   void encode(DBusMessageIter& iter, const std::variant<T...>& v)
   {
      detail::VariantSerializer vs(iter);
      std::visit(vs, const_cast<std::variant<T...>&>(v));   // TODO need const visitor
   }


   static
   void decode(DBusMessageIter& orig, std::variant<T...>& v)
   {
      DBusMessageIter iter;
      simppl_dbus_message_iter_recurse(&orig, &iter, DBUS_TYPE_VARIANT);

      std::unique_ptr<char, void(*)(void*)> sig(dbus_message_iter_get_signature(&iter), &dbus_free);

      if (!detail::try_deserialize(iter, v, sig.get()))
         assert(false);

      dbus_message_iter_next(&orig);
   }
};


template<typename... T>
bool detail::try_deserialize(DBusMessageIter& iter, std::variant<T...>& v, const char* sig)
{
   // the first alternative with matching signature wins
   return ((!strcmp(signature_of<T>(), sig) && (Codec<T>::decode(iter, v.template emplace<T>()), true)) || ...);
}


template<typename T>
inline
void detail::VariantSerializer::operator()(const T& t)   // seems to be already a reference so no copy is done
{
    DBusMessageIter iter;
    dbus_message_iter_open_container(&iter_, DBUS_TYPE_VARIANT, signature_of<T>(), &iter);

    Codec<T>::encode(iter, t);

    dbus_message_iter_close_container(&iter_, &iter);
}


}   // namespace dbus

}   // namespace simppl


#endif  // SIMPPL_VARIANT_H
