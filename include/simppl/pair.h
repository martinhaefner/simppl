#ifndef __SIMPPL_DBUS_PAIR_H__
#define __SIMPPL_DBUS_PAIR_H__


#include <map>


namespace simppl
{

namespace dbus
{

namespace detail
{
 
   
template<typename KeyT, typename ValueT>
struct Codec<std::pair<KeyT, ValueT>>
{
   static 
   void encode(Serializer& s, const std::pair<KeyT, ValueT>& p)
   {
      DBusMessageIter item_iterator;
      dbus_message_iter_open_container(s.iter_, DBUS_TYPE_DICT_ENTRY, nullptr, &item_iterator);

      Serializer s1(&item_iterator);
      Codec<KeyT>::encode(s1, p.first);
      Codec<ValueT>::encode(s1, p.second);
      
      dbus_message_iter_close_container(s.iter_, &item_iterator);
   }
   
   
   static 
   void decode(Deserializer& s, std::pair<KeyT, ValueT>& p)
   {
      DBusMessageIter item_iterator;
      dbus_message_iter_recurse(s.iter_, &item_iterator);

      Deserializer s1(&item_iterator);
      Codec<KeyT>::decode(s1, p.first);
      Codec<ValueT>::decode(s1, p.second);
      
      // advance to next element
      dbus_message_iter_next(s.iter_);
   }
};


template<typename KeyT, typename ValueT>
struct make_type_signature<std::pair<KeyT, ValueT>>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      os << DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING;
      make_type_signature<KeyT>::eval(os);
      make_type_signature<ValueT>::eval(os);
      return os << DBUS_DICT_ENTRY_END_CHAR_AS_STRING;
   }
};

   
}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // __SIMPPL_DBUS_PAIR_H__
