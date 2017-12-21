

template<typename T>
struct isPod
{
   typedef make_typelist<
      bool,
      char,
      signed char,
      unsigned char,
      short,
      unsigned short,
      int,
      unsigned int,
      long,
      unsigned long,
      long long,
      unsigned long long,
      float,
      double>::type pod_types;

  enum { value = Find<T, pod_types>::value >= 0 };
};


template<typename T>
   Deserializer& read(T& t, std::true_type)
   {
      dbus_message_iter_get_basic(iter_, &t);
      dbus_message_iter_next(iter_);

      return *this;
   }


 template<typename T>
   inline
   Serializer& write(T t, Pod)
   {
      dbus_message_iter_append_basic(iter_, dbus_type_code<T>::value, &t);
      return *this;
   }
  

template<> struct dbus_type_code<char>               { enum { value = DBUS_TYPE_BYTE    }; };
template<> struct dbus_type_code<uint8_t>            { enum { value = DBUS_TYPE_BYTE    }; };
template<> struct dbus_type_code<uint16_t>           { enum { value = DBUS_TYPE_UINT16  }; };
template<> struct dbus_type_code<uint32_t>           { enum { value = DBUS_TYPE_UINT32  }; };
//template<> struct dbus_type_code<uint64_t> { enum { value = DBUS_TYPE_UINT64  }; };
template<> struct dbus_type_code<unsigned long>      { enum { value = DBUS_TYPE_UINT32   }; };
template<> struct dbus_type_code<unsigned long long> { enum { value = DBUS_TYPE_UINT64   }; };
template<> struct dbus_type_code<int8_t>             { enum { value = DBUS_TYPE_BYTE    }; };
template<> struct dbus_type_code<int16_t>            { enum { value = DBUS_TYPE_INT16   }; };
template<> struct dbus_type_code<int32_t>            { enum { value = DBUS_TYPE_INT32   }; };
//template<> struct dbus_type_code<int64_t>  { enum { value = DBUS_TYPE_INT64   }; };
template<> struct dbus_type_code<long>               { enum { value = DBUS_TYPE_INT32   }; };
template<> struct dbus_type_code<long long>          { enum { value = DBUS_TYPE_INT64   }; };
template<> struct dbus_type_code<double>             { enum { value = DBUS_TYPE_DOUBLE  }; };
