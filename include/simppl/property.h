#ifndef SIMPPL_PROPERTY_H
#define SIMPPL_PROPERTY_H


#include "simppl/variant.h"
#include "simppl/calltraits.h"


namespace simppl
{

namespace dbus
{

enum PropertyFlags
{
   ReadOnly     = 0,
   OnChange     = 0,

   ReadWrite    = (1<<0),
   Notifying    = (1<<1),
   Always       = (1<<2)
};


/// used for OnChange properties
/// FIXME string and wstring would make trouble as attribute!
template<typename T, bool do_compare>
struct PropertyComparator
{
    /// compare property types with operator != returning true if t1 and t2 are
    /// different.
    static inline
    bool compare(typename CallTraits<T>::param_type t1, typename CallTraits<T>::param_type t2)
    {
        return t1 != t2;
    }
};


/// used for Always properties
template<typename T>
struct PropertyComparator<T, false>
{
    /// always evaluate to true, i.e. properties will emit even if no
    /// change is given.
    static inline
    bool compare(typename CallTraits<T>::param_type t1, typename CallTraits<T>::param_type t2)
    {
        return true;
    }
};


namespace detail
{


/**
 * Wraps the property type in a variant.
 */
template<typename T>
struct PropertyCodec
{
   static
   void encode(Encoder& e, const T& t)
   {
      detail::VariantSerializer(e).operator()(t);
   }


   static
   void decode(Decoder& d, T& t)
   {
      Decoder variant = d.recurse(DBUS_TYPE_VARIANT);

      detail::decode_one<T>(variant, t);

      d.next();
   }
};


}   // detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_PROPERTY_H
