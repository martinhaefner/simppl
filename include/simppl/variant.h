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
struct VariantDeserializer;

template<typename T1, typename... T>
struct VariantDeserializer<T1, T...>
{
   template<typename VariantT>
   static
   bool eval(DBusMessageIter& iter, VariantT& v, const char* sig)
   {
      std::ostringstream buf;
      Codec<T1>::make_type_signature(buf);

      if (!strcmp(buf.str().c_str(), sig))
      {
         v = T1();
         Codec<T1>::decode(iter, std::get<T1>(v));

         return true;
      }
      else
         return VariantDeserializer<T...>::eval(iter, v, sig);
   }
};


template<typename T>
struct VariantDeserializer<T>
{
   template<typename VariantT>
   static
   bool eval(DBusMessageIter& iter, VariantT& v, const char* sig)
   {
      std::ostringstream buf;
      Codec<T>::make_type_signature(buf);

      if (!strcmp(buf.str().c_str(), sig))
      {
         v = T();
         Codec<T>::decode(iter, std::get<T>(v));

         return true;
      }

      // stop recursion
      return false;
   }
};


template<typename... T>
bool try_deserialize(DBusMessageIter& iter, std::variant<T...>& v, const char* sig);


}   // namespace detail


template<typename... T>
struct Codec<std::variant<T...>>
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


   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return os << DBUS_TYPE_VARIANT_AS_STRING;
   }
};


template<typename... T>
bool detail::try_deserialize(DBusMessageIter& iter, std::variant<T...>& v, const char* sig)
{
   return VariantDeserializer<T...>::eval(iter, v, sig);
}


template<typename T>
inline
void detail::VariantSerializer::operator()(const T& t)   // seems to be already a reference so no copy is done
{
    std::ostringstream buf;
    Codec<T>::make_type_signature(buf);

    DBusMessageIter iter;
    dbus_message_iter_open_container(&iter_, DBUS_TYPE_VARIANT, buf.str().c_str(), &iter);

    Codec<T>::encode(iter, t);

    dbus_message_iter_close_container(&iter_, &iter);
}


}   // namespace dbus

}   // namespace simppl


#endif  // SIMPPL_VARIANT_H
