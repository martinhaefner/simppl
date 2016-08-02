#ifndef SIMPPL_DETAIL_SERIALIZATION_H
#define SIMPPL_DETAIL_SERIALIZATION_H


#include "simppl/noninstantiable.h"
#include "simppl/typelist.h"
#include "simppl/callstate.h"
#include "simppl/variant.h"

#include <map>
#include <vector>
#include <tuple>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <tuple>
#include <sstream>
#include <stdint.h>

#include <dbus/dbus.h>

#ifdef SIMPPL_HAVE_BOOST_FUSION
#   include <boost/fusion/support/is_sequence.hpp>
#   include <boost/fusion/algorithm.hpp>
#endif


namespace simppl
{

namespace dbus
{

namespace detail
{

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


// --------------------------------------------------------------------------------------


// generic type switch
template<typename T> struct dbus_type_code
{ enum { value = std::is_enum<T>::value ? DBUS_TYPE_INT32 : DBUS_TYPE_STRUCT }; };

template<> struct dbus_type_code<bool>               { enum { value = DBUS_TYPE_BOOLEAN }; };
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

template<typename... T>
struct dbus_type_code<std::tuple<T...>>              { enum { value = DBUS_TYPE_STRUCT }; };

template<typename... T>
struct dbus_type_code<Variant<T...>>                 { enum { value = DBUS_TYPE_VARIANT }; };


// forward decl
template<typename T>
struct make_type_signature;


// --------------------------------------------------------------------------------------


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


// --------------------------------------------------------------------------------------------


// FIXME remove this in favour of lambda expression!
template<typename SerializerT>
struct TupleSerializer // : noncopable
{
   inline
   TupleSerializer(SerializerT& s)
    : orig_(s)
    , s_(&iter_)
   {
      dbus_message_iter_open_container(orig_.iter_, DBUS_TYPE_STRUCT, nullptr, &iter_);
   }

   inline
   ~TupleSerializer()
   {
      dbus_message_iter_close_container(orig_.iter_, &iter_);
   }

   template<typename T>
   void operator()(const T& t);

   SerializerT& orig_;
   SerializerT s_;
   DBusMessageIter iter_;
};


template<typename DeserializerT>
struct TupleDeserializer // : noncopable
{
   inline
   TupleDeserializer(DeserializerT& s, bool flattened = false)
    : orig_(s)
    , s_(flattened?orig_.iter_:&iter_)
    , flattened_(flattened)
   {
      if (!flattened)
         dbus_message_iter_recurse(orig_.iter_, &iter_);
   }

   ~TupleDeserializer()
   {
      if (!flattened_)
         dbus_message_iter_next(orig_.iter_);
   }

   template<typename T>
   void operator()(T& t);

   DeserializerT& orig_;
   DeserializerT s_;
   DBusMessageIter iter_;
   bool flattened_;
};


// ------------------------------------------------------------------------------------


template<typename ListT>
struct make_serializer_imp;

template<typename T1, typename ListT>
struct make_serializer_imp<TypeList<T1, ListT> >
{
   typedef SerializerTuple<typename make_serializer_imp<TypeList<T1, typename PopBack<ListT>::type> >::type, typename Back<ListT>::type> type;
};

template<typename T>
struct make_serializer_imp<TypeList<T, NilType> >
{
   typedef SerializerTuple<T, NilType> type;
};


// ------------------------------------------------------------------------------------


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


// ---------------------------------------------------------------------


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


// ------------------------------------------------------------------------------------


template<size_t N>
struct StdTupleForEach
{
   template<typename TupleT, typename FunctorT>
   static inline
   void eval(TupleT& t, FunctorT func)
   {
     enum { __M = std::tuple_size<typename std::remove_const<TupleT>::type>::value - N };
      func(std::get<__M>(t));
      StdTupleForEach<N-1>::template eval(t, func);
   }
};

template<>
struct StdTupleForEach<1>
{
   template<typename TupleT, typename FunctorT>
   static inline
   void eval(TupleT& t, FunctorT func)
   {
     enum { __M = std::tuple_size<typename std::remove_const<TupleT>::type>::value - 1 };
      func(std::get<__M>(t));
   }
};


template<typename TupleT, typename FunctorT>
inline
void std_tuple_for_each(TupleT& t, FunctorT functor)
{
   StdTupleForEach<std::tuple_size<typename std::remove_const<TupleT>::type>::value>::template eval(t, functor);
}


// ---------------------------------------------------------------------


template<typename T>
struct make_type_signature
{
   static inline std::ostream& eval(std::ostream& os)
   {
      make_type_signature<T>::helper(os, bool_constant<isPod<T>::value || std::is_enum<T>::value>());
      return os;
   }


private:

   // pod helper
   static inline void helper(std::ostream& os, std::true_type)
   {
      os << (char)dbus_type_code<T>::value;
   }

   // struct helper
   static inline void helper(std::ostream& os, std::false_type)
   {
      T::serializer_type::write_signature(os << DBUS_STRUCT_BEGIN_CHAR_AS_STRING);
      os << DBUS_STRUCT_END_CHAR_AS_STRING;
   }
};


template<typename... T>
struct make_type_signature<std::tuple<T...>>
{
   // if templated lambdas are available this could be removed!
   struct helper
   {
      helper(std::ostream& os)
       : os_(os)
      {
         // NOOP
      }

      template<typename U>
      void operator()(const U&)
      {
         make_type_signature<U>::eval(os_);
      }

      std::ostream& os_;
   };

   static inline
   std::ostream& eval(std::ostream& os)
   {
      os << DBUS_STRUCT_BEGIN_CHAR_AS_STRING;
      std::tuple<T...> t;   // FIXME make this a type based version only, no value based iteration
      std_tuple_for_each(t, helper(os));
      os << DBUS_STRUCT_END_CHAR_AS_STRING;

      return os;
   }
};


template<typename... T>
struct make_type_signature<simppl::Variant<T...>>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return os << DBUS_TYPE_VARIANT_AS_STRING;
   }
};


template<typename T>
struct make_type_signature<std::vector<T>>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return make_type_signature<T>::eval(os << DBUS_TYPE_ARRAY_AS_STRING);
   }
};


template<>
struct make_type_signature<std::string>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return os << DBUS_TYPE_STRING_AS_STRING;
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


template<typename KeyT, typename ValueT>
struct make_type_signature<std::map<KeyT, ValueT>>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return make_type_signature<std::pair<typename std::decay<KeyT>::type, ValueT>>::eval(os << DBUS_TYPE_ARRAY_AS_STRING);
   }
};


// --- variant stuff ---------------------------------------------------


template<typename SerializerT>
struct VariantSerializer : StaticVisitor<>
{
   inline
   VariantSerializer(SerializerT& s)
    : orig_(s)
   {
       // NOOP
   }

   template<typename T>
   void operator()(const T& t);

   SerializerT& orig_;
};


template<typename... T>
struct VariantDeserializer;

template<typename T1, typename... T>
struct VariantDeserializer<T1, T...>
{
   template<typename DeserializerT, typename VariantT>
   static bool eval(DeserializerT& s, VariantT& v, const char* sig)
   {
      std::ostringstream buf;
      make_type_signature<T1>::eval(buf);

      if (!strcmp(buf.str().c_str(), sig))
      {
         v = T1();
         s >> *v.template get<T1>();

         return true;
      }
      else
         return VariantDeserializer<T...>::eval(s, v, sig);
   }
};


template<typename T>
struct VariantDeserializer<T>
{
   template<typename DeserializerT, typename VariantT>
   static bool eval(DeserializerT& s, VariantT& v, const char* sig)
   {
      std::ostringstream buf;
      make_type_signature<T>::eval(buf);

      if (!strcmp(buf.str().c_str(), sig))
      {
         v = T();
         s >> *v.template get<T>();

         return true;
      }

      // stop recursion
      return false;
   }
};


template<typename DeserializerT, typename... T>
bool try_deserialize(DeserializerT& d, Variant<T...>& v, const char* sig);


// ---------------------------------------------------------------------


struct Serializer // : noncopyable
{
   explicit
   Serializer(DBusMessage* msg);

   explicit
   Serializer(DBusMessageIter* iter);

   template<typename T>
   inline
   Serializer& write(const T& t)
   {
      return write(t, bool_constant<isPod<T>::value || std::is_pointer<T>::value || std::is_enum<T>::value>());
   }

   template<typename T>
   inline
   Serializer& write(T t, std::true_type)
   {
      dbus_message_iter_append_basic(iter_, dbus_type_code<T>::value, &t);
      return *this;
   }

   template<typename T>
   inline
   Serializer& write(const T& t, std::false_type)
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


   Serializer& write(const std::string& str);
   Serializer& write(const char* str);

   template<typename... T>
   inline
   Serializer& write(const Variant<T...>& v)
   {
       VariantSerializer<Serializer> s(*this);
       staticVisit(s, const_cast<Variant<T...>&>(v));   // FIXME need const visitor
       return *this;
   }

   template<typename T>
   inline
   Serializer& write(const std::vector<T>& v)
   {
      DBusMessageIter iter;
      std::ostringstream buf;
      make_type_signature<T>::eval(buf);

      dbus_message_iter_open_container(iter_, DBUS_TYPE_ARRAY, buf.str().c_str(), &iter);

      Serializer s(&iter);
      for (auto& t : v) {
         s.write(t);
      }

      dbus_message_iter_close_container(iter_, &iter);

      return *this;
   }

   template<typename KeyT, typename ValueT>
   Serializer& write(const std::map<KeyT, ValueT>& m)
   {
      std::ostringstream buf;
      make_type_signature<std::pair<KeyT, ValueT>>::eval(buf);

      DBusMessageIter iter;
      dbus_message_iter_open_container(iter_, DBUS_TYPE_ARRAY, buf.str().c_str(), &iter);

      Serializer s(&iter);

      for (auto& e : m) {
         s.write(e);
      }

      dbus_message_iter_close_container(iter_, &iter);

      return *this;
   }

   template<typename... T>
   Serializer& write(const std::tuple<T...>& t)
   {
      TupleSerializer<Serializer> ts(*this);
      std_tuple_for_each(t, std::ref(ts));
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


private:

   template<typename KeyT, typename ValueT>
   inline
   void write(const std::pair<KeyT, ValueT>& p)
   {
      DBusMessageIter item_iterator;
      dbus_message_iter_open_container(iter_, DBUS_TYPE_DICT_ENTRY, nullptr, &item_iterator);

      Serializer s(&item_iterator);
      s.write(p.first);
      s.write(p.second);

      dbus_message_iter_close_container(iter_, &item_iterator);
   }

   DBusMessageIter private_iter_;


public:

   DBusMessageIter* iter_;
};

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#define MAKE_SERIALIZER(type) \
inline \
simppl::dbus::detail::Serializer& operator<<(simppl::dbus::detail::Serializer& s, type t) \
{ \
   return s.write(t); \
}

MAKE_SERIALIZER(bool)

MAKE_SERIALIZER(unsigned short)
MAKE_SERIALIZER(short)

MAKE_SERIALIZER(unsigned int)
MAKE_SERIALIZER(int)

MAKE_SERIALIZER(unsigned long)
MAKE_SERIALIZER(long)

MAKE_SERIALIZER(unsigned long long)
MAKE_SERIALIZER(long long)

MAKE_SERIALIZER(double)

MAKE_SERIALIZER(const char*)
MAKE_SERIALIZER(const std::string&)


template<typename T>
inline
simppl::dbus::detail::Serializer& operator<<(simppl::dbus::detail::Serializer& s, const std::vector<T>& v)
{
   return s.write(v);
}


template<typename KeyT, typename ValueT>
inline
simppl::dbus::detail::Serializer& operator<<(simppl::dbus::detail::Serializer& s, const std::map<KeyT, ValueT>& m)
{
   return s.write(m);
}


template<typename StructT>
inline
simppl::dbus::detail::Serializer& operator<<(simppl::dbus::detail::Serializer& s, const StructT& st)
{
   return s.write(st);
}


template<typename... T>
inline
simppl::dbus::detail::Serializer& operator<<(simppl::dbus::detail::Serializer& s, const std::tuple<T...>& t)
{
   return s.write(t);
}


template<typename SerializerT>
template<typename T>
inline
void simppl::dbus::detail::TupleSerializer<SerializerT>::operator()(const T& t)   // seems to be already a reference so no copy is done
{
    s_ << t;
}


template<typename SerializerT>
template<typename T>
inline
void simppl::dbus::detail::VariantSerializer<SerializerT>::operator()(const T& t)   // seems to be already a reference so no copy is done
{
    std::ostringstream buf;
    make_type_signature<T>::eval(buf);

    DBusMessageIter iter;
    dbus_message_iter_open_container(orig_.iter_, DBUS_TYPE_VARIANT, buf.str().c_str(), &iter);

    SerializerT s(&iter);
    s.write(t);

    dbus_message_iter_close_container(orig_.iter_, &iter);
}


// -----------------------------------------------------------------------------


namespace simppl
{

namespace dbus
{

namespace detail
{

struct Deserializer // : noncopyable
{
   static inline
   void free(char* ptr)
   {
      delete[] ptr;
   }

   explicit
   Deserializer(DBusMessage* msg);

   explicit
   Deserializer(DBusMessageIter* iter);

   template<typename T>
   inline
   Deserializer& read(T& t)
   {
      return read(t, bool_constant<isPod<T>::value || std::is_enum<T>::value>());
   }

   template<typename T>
   Deserializer& read(T& t, std::true_type)
   {
      dbus_message_iter_get_basic(iter_, &t);
      dbus_message_iter_next(iter_);

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
   }

   Deserializer& read(char*& str);

   Deserializer& read(std::string& str);

   template<typename... T>
   inline
   Deserializer& read(Variant<T...>& v)
   {
       DBusMessageIter iter;
       dbus_message_iter_recurse(iter_, &iter);
       Deserializer s(&iter);

       //if (!try_deserialize(s, v, dbus_message_iter_get_signature(&iter)))
         //std::cerr << "Invalid variant type detected" << std::endl;

       dbus_message_iter_next(iter_);
       return *this;
   }

   template<typename T>
   Deserializer& read(std::vector<T>& v)
   {
      v.clear();

      DBusMessageIter iter;
      dbus_message_iter_recurse(iter_, &iter);

      Deserializer s(&iter);

      while(dbus_message_iter_get_arg_type(&iter) != 0)
      {
         T t;
         s.read(t);
         v.push_back(t);
      }

      // advance to next element
      dbus_message_iter_next(iter_);

      return *this;
   }


   template<typename KeyT, typename ValueT>
   inline
   void read(std::pair<KeyT, ValueT>& p)
   {
      DBusMessageIter item_iterator;
      dbus_message_iter_recurse(iter_, &item_iterator);

      Deserializer s(&item_iterator);
      s.read(p.first);
      s.read(p.second);

      // advance to next element
      dbus_message_iter_next(iter_);
   }


   template<typename KeyT, typename ValueT>
   Deserializer& read(std::map<KeyT, ValueT>& m)
   {
      m.clear();

      DBusMessageIter iter;
      dbus_message_iter_recurse(iter_, &iter);

      Deserializer s(&iter);

      while(dbus_message_iter_get_arg_type(&iter) != 0)
      {
         std::pair<KeyT, ValueT> p;
         s.read(p);

         m.insert(p);
      }

      // advance to next element
      dbus_message_iter_next(iter_);

      return *this;
   }

   template<typename... T>
   Deserializer& read(std::tuple<T...>& t)
   {
      TupleDeserializer<Deserializer> tds(*this);
      std_tuple_for_each(t, std::ref(tds));
      return *this;
   }

   template<typename... T>
   Deserializer& read_flattened(std::tuple<T...>& t)
   {
      TupleDeserializer<Deserializer> tds(*this, true);
      std_tuple_for_each(t, std::ref(tds));
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

private:

   static inline
   char* allocate(size_t len)
   {
      return new char[len];
   }

   DBusMessageIter private_iter_;

public:   // FIXME friend?

   DBusMessageIter* iter_;
};

}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#define MAKE_DESERIALIZER(type) \
inline \
simppl::dbus::detail::Deserializer& operator>>(simppl::dbus::detail::Deserializer& s, type& t) \
{ \
   return s.read(t); \
}

MAKE_DESERIALIZER(bool)

MAKE_DESERIALIZER(unsigned short)
MAKE_DESERIALIZER(short)

MAKE_DESERIALIZER(unsigned int)
MAKE_DESERIALIZER(int)

MAKE_DESERIALIZER(unsigned long)
MAKE_DESERIALIZER(long)

MAKE_DESERIALIZER(unsigned long long)
MAKE_DESERIALIZER(long long)

MAKE_DESERIALIZER(double)

MAKE_DESERIALIZER(char*)
MAKE_DESERIALIZER(std::string)


template<typename T>
inline
simppl::dbus::detail::Deserializer& operator>>(simppl::dbus::detail::Deserializer& s, std::vector<T>& v)
{
   return s.read(v);
}

template<typename KeyT, typename ValueT>
inline
simppl::dbus::detail::Deserializer& operator>>(simppl::dbus::detail::Deserializer& s, std::map<KeyT, ValueT>& m)
{
   return s.read(m);
}

template<typename StructT>
inline
simppl::dbus::detail::Deserializer& operator>>(simppl::dbus::detail::Deserializer& s, StructT& st)
{
   return s.read(st);
}

template<typename... T>
inline
simppl::dbus::detail::Deserializer& operator>>(simppl::dbus::detail::Deserializer& s, std::tuple<T...>& t)
{
   return s.read(t);
}


template<typename DeserializerT>
template<typename T>
inline
void simppl::dbus::detail::TupleDeserializer<DeserializerT>::operator()(T& t)
{
   s_ >> t;
}


template<typename DeserializerT, typename... T>
bool simppl::dbus::detail::try_deserialize(DeserializerT& d, Variant<T...>& v, const char* sig)
{
   return simppl::dbus::detail::VariantDeserializer<T...>::eval(d, v, sig);
}


// ------------------------------------------------------------------------


namespace simppl
{

namespace dbus
{

namespace detail
{

inline
Serializer& serialize(Serializer& s)
{
   return s;
}

template<typename T1, typename... T>
inline
Serializer& serialize(Serializer& s, const T1& t1, const T&... t)
{
   s << t1;
   return serialize(s, t...);
}


}   // namespace detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERIALIZATION_H
