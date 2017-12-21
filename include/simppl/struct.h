

template<typename T1, typename T2>
struct SerializerTuple : T1
{
   template<typename SerializerT>
   void write(SerializerT& s) const
   {
      T1::write(s);
      s.write(data_);
   }

   template<typename DeserializerT>
   void read(DeserializerT& s)
   {
      T1::read(s);
      s.read(data_);
   }

   static
   void write_signature(std::ostream& os)
   {
      T1::write_signature(os);
      make_type_signature<T2>::eval(os);
   }

   T2 data_;
};


template<typename T>
struct SerializerTuple<T, NilType>
{
   template<typename SerializerT>
   void write(SerializerT& s) const
   {
      s.write(data_);
   }

   template<typename DeserializerT>
   void read(DeserializerT& s)
   {
      s.read(data_);
   }

   static
   void write_signature(std::ostream& os)
   {
      make_type_signature<T>::eval(os);
   }

   T data_;
};


template<typename T>
struct StructSerializationHelper
{
   template<typename SerializerT, typename StructT>
   static inline
   void write(SerializerT& s, const StructT& st)
   {
      const typename StructT::serializer_type& tuple = *(const typename StructT::serializer_type*)&st;
      s.write(tuple);
   }

   template<typename DeserializerT, typename StructT>
   static inline
   void read(DeserializerT& s, StructT& st)
   {
      typename StructT::serializer_type& tuple = *(typename StructT::serializer_type*)&st;
      s.read(tuple);
   }
};


#ifdef SIMPPL_HAVE_BOOST_FUSION
template<typename SerializerT>
struct FusionWriter
{
   explicit inline
   FusionWriter(SerializerT& s)
    : s_(s)
   {
      // NOOP
   }

   template<typename T>
   inline
   void operator()(const T& t) const
   {
      s_.write(t);
   }

   SerializerT& s_;
};


template<typename DeserializerT>
struct FusionReader
{
   explicit inline
   FusionReader(DeserializerT& s)
    : s_(s)
   {
      // NOOP
   }

   template<typename T>
   inline
   void operator()(T& t) const
   {
      s_.read(t);
   }

   DeserializerT& s_;
};


template<>
struct StructSerializationHelper<boost::mpl::true_>
{
   template<typename SerializerT, typename StructT>
   static inline
   void write(SerializerT& s, const StructT& st)
   {
      boost::fusion::for_each(st, FusionWriter<SerializerT>(s));
   }

   template<typename DeserializerT, typename StructT>
   static inline
   void read(DeserializerT& s, StructT& st)
   {
      boost::fusion::for_each(st, FusionReader<DeserializerT>(s));
   }
};
#endif


template<typename T>
   inline
   Serializer& write(const T& t, Struct)
   {
      StructSerializationHelper<
#ifdef SIMPPL_HAVE_BOOST_FUSION
         typename boost::fusion::traits::is_sequence<T>::type
#else
         int
#endif
         >::template write(*this, t);

      return *this;
   }


 template<typename T1, typename T2>
   Serializer& write(const SerializerTuple<T1, T2>& tuple)
   {
      DBusMessageIter iter;
      dbus_message_iter_open_container(iter_, DBUS_TYPE_STRUCT, nullptr, &iter);

      Serializer sub(&iter);
      tuple.write(sub);

      dbus_message_iter_close_container(iter_, &iter);

      return *this;
   }


template<typename T>
   Deserializer& read(T& t, std::false_type)
   {
      StructSerializationHelper<
#ifdef SIMPPL_HAVE_BOOST_FUSION
         typename boost::fusion::traits::is_sequence<T>::type
#else
         int
#endif
         >::template read(*this, t);
      return *this;
   }


 template<typename T1, typename T2>
   Deserializer& read(SerializerTuple<T1, T2>& tuple)
   {
      DBusMessageIter iter;
      dbus_message_iter_recurse(iter_, &iter);

      Deserializer sub(&iter);
      tuple.read(sub);

      dbus_message_iter_next(iter_);

      return *this;
   }
