#ifndef SIMPPL_DBUS_POD_H
#define SIMPPL_DBUS_POD_H

#include <dbus/dbus-protocol.h>
#ifndef SIMPPL_SERIALIZATION_H
#   error "Do not include this file manually. Use serialization.h instead."
#endif

#include <cstdint>
#include <vector>
#include <string>

namespace simppl
{
   
namespace dbus
{
   

namespace detail
{

template<typename T>
struct typecode_switch;

template<> struct typecode_switch<uint8_t>            { enum { value = DBUS_TYPE_BYTE    }; };
template<> struct typecode_switch<uint16_t>           { enum { value = DBUS_TYPE_UINT16  }; };
template<> struct typecode_switch<uint32_t>           { enum { value = DBUS_TYPE_UINT32  }; };
template<> struct typecode_switch<uint64_t>           { enum { value = DBUS_TYPE_UINT64  }; };
template<> struct typecode_switch<int8_t>             { enum { value = DBUS_TYPE_BYTE    }; };
template<> struct typecode_switch<int16_t>            { enum { value = DBUS_TYPE_INT16   }; };
template<> struct typecode_switch<int32_t>            { enum { value = DBUS_TYPE_INT32   }; };
template<> struct typecode_switch<int64_t>            { enum { value = DBUS_TYPE_INT64   }; };
template<> struct typecode_switch<double>             { enum { value = DBUS_TYPE_DOUBLE  }; };
template<> struct typecode_switch<std::string>        { enum { value = DBUS_TYPE_STRING  }; };
template<typename T, typename Alloc>
struct typecode_switch<std::vector<T, Alloc>>         { enum { value = DBUS_TYPE_ARRAY  }; };


template<typename T, bool>
struct EnumTypeCodeHelper
{
   enum { value = DBUS_TYPE_INT32 };
};

template<typename T>
struct EnumTypeCodeHelper<T, false>
{
   enum { value = typecode_switch<T>::value };
};


}   // namespace detail


template<typename T>
struct CodecImpl<T, Pod>
{
   enum { dbus_type_code = detail::EnumTypeCodeHelper<T, std::is_enum<T>::value>::value };
      
   static_assert(isPod<T>::value || std::is_enum<T>::value, "not a pod type");
      
      
   static inline
   void encode(DBusMessageIter& iter, const T& t)
   {
      dbus_message_iter_append_basic(&iter, dbus_type_code, &t);
   }
   
   
   static inline
   void decode(DBusMessageIter& iter, T& t)
   {
      simppl_dbus_message_iter_get_basic(&iter, &t, dbus_type_code);
   }
   
   
   static inline
   std::ostream& make_type_signature(std::ostream& os)
   {
      return os << (char)dbus_type_code;
   }
};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_POD_H
