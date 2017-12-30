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
   void encode(DBusMessageIter& iter, const T& t)
   {
      std::ostringstream buf;
      Codec<T>::make_type_signature(buf);

      DBusMessageIter _iter;
      dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, buf.str().c_str(), &_iter);

      Codec<T>::encode(_iter, t);
      
      dbus_message_iter_close_container(&iter, &_iter);
   }
   
   
   static 
   void decode(DBusMessageIter& iter, T& t)
   {
      DBusMessageIter _iter;
      dbus_message_iter_recurse(&iter, &_iter);

      Codec<T>::decode(_iter, t);

      dbus_message_iter_next(&iter);
   } 
};


}   // detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_PROPERTY_H
