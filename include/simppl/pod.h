#ifndef SIMPPL_DBUS_POD_H
#define SIMPPL_DBUS_POD_H

#include <dbus/dbus-protocol.h>
#ifndef SIMPPL_SERIALIZATION_H
#   error "Do not include this file manually. Use serialization.h instead."
#endif

#include <cstdint>

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
 : composite_signature<signature_chars<detail::EnumTypeCodeHelper<T, std::is_enum<T>::value>::value>>
{
   enum { dbus_type_code = detail::EnumTypeCodeHelper<T, std::is_enum<T>::value>::value };
      
   static_assert(isPod<T>::value || std::is_enum<T>::value, "not a pod type");
      
      
   static inline
   void encode(Encoder& e, const T& t)
   {
      e.append_basic(dbus_type_code, &t);
   }
   
   
   static inline
   void decode(Decoder& d, T& t)
   {
      d.get_basic(&t, dbus_type_code);
   }
};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DBUS_POD_H
