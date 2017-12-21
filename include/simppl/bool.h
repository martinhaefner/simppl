


inline
   Serializer& write(bool b)
   {
      dbus_bool_t _b = b;
      dbus_message_iter_append_basic(iter_, dbus_type_code<bool>::value, &_b);
      return *this;
   }



Deserializer& read(bool& t)
   {
      dbus_bool_t b;
      dbus_message_iter_get_basic(iter_, &b);
      dbus_message_iter_next(iter_);

      t = b;
      return *this;
   }


template<> struct dbus_type_code<bool>               { enum { value = DBUS_TYPE_BOOLEAN }; };
