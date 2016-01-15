#ifndef SIMPPL_DETAIL_SERIALIZATION_H
#define SIMPPL_DETAIL_SERIALIZATION_H


#include "simppl/noninstantiable.h"
#include "simppl/typelist.h"
#include "simppl/callstate.h"

#include <map>
#include <vector>
#include <tuple>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <tuple>
#include <iostream>   // FIXME remove this
#include <stdint.h>

#include <dbus/dbus.h>

#ifdef SIMPPL_HAVE_BOOST_FUSION
#   include <boost/fusion/support/is_sequence.hpp>
#   include <boost/fusion/algorithm.hpp>
#endif


namespace simppl
{

namespace ipc
{

// forward decls
template<typename> struct ServerVectorAttributeUpdate;
template<typename> struct ClientVectorAttributeUpdate;


namespace detail
{

template<typename T>
struct isPod
{
   typedef TYPELIST_14( \
      bool, \
      char, \
      signed char, \
      unsigned char, \
      short, \
      unsigned short, \
      int, \
      unsigned int, \
      long, \
      unsigned long, \
      long long, \
      unsigned long long, \
      float, \
      double) pod_types;

  enum { value = Find<T, pod_types>::value >= 0 };
};


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

   T data_;
};


// --------------------------------------------------------------------------------------------


template<typename SerT>
struct VectorSerializer
{
   inline
   VectorSerializer(SerT& s)
    : s_(s)
   {
      // NOOP
   }

   template<typename T>
   inline
   void operator()(const T& t)
   {
      s_.write(t);
   }

   SerT& s_;
};


template<typename SerializerT>
struct TupleSerializer // : noncopable
{
   TupleSerializer(SerializerT& s)
    : orig_(s)
    , s_(&s.iter_)
   {
      // NOOP
   }
   
   template<typename T>
   void operator()(const T& t);

   SerializerT& orig_;
   SerializerT s_;
};


template<typename DeserializerT>
struct TupleDeserializer // : noncopable
{
   TupleDeserializer(DeserializerT& s)
    : orig_(s)
    , s_(&s.iter_)
   {
      // NOOP
   }

   template<typename T>
   void operator()(T& t);

   DeserializerT& orig_;
   DeserializerT s_;
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


// ------------------------------------------------------------------------------------


// forward decl
template<typename T> struct dbus_type_code;

template<> struct dbus_type_code<bool>     { enum { value = DBUS_TYPE_BOOLEAN }; };
template<> struct dbus_type_code<char>     { enum { value = DBUS_TYPE_BYTE    }; };
template<> struct dbus_type_code<uint8_t>  { enum { value = DBUS_TYPE_BYTE    }; };
template<> struct dbus_type_code<uint16_t> { enum { value = DBUS_TYPE_UINT16  }; };
template<> struct dbus_type_code<uint32_t> { enum { value = DBUS_TYPE_UINT32  }; };
//template<> struct dbus_type_code<uint64_t> { enum { value = DBUS_TYPE_UINT64  }; };
template<> struct dbus_type_code<unsigned long> { enum { value = DBUS_TYPE_UINT32   }; };
template<> struct dbus_type_code<unsigned long long> { enum { value = DBUS_TYPE_UINT64   }; };
template<> struct dbus_type_code<int8_t>   { enum { value = DBUS_TYPE_BYTE    }; };
template<> struct dbus_type_code<int16_t>  { enum { value = DBUS_TYPE_INT16   }; };
template<> struct dbus_type_code<int32_t>  { enum { value = DBUS_TYPE_INT32   }; };
//template<> struct dbus_type_code<int64_t>  { enum { value = DBUS_TYPE_INT64   }; };
template<> struct dbus_type_code<long> { enum { value = DBUS_TYPE_INT32   }; };
template<> struct dbus_type_code<long long> { enum { value = DBUS_TYPE_INT64   }; };
template<> struct dbus_type_code<double>   { enum { value = DBUS_TYPE_DOUBLE  }; };


struct Serializer // : noncopyable
{
   explicit
   Serializer(DBusMessage* msg);

   explicit
   Serializer(DBusMessageIter* iter);

   ~Serializer();

   template<typename T>
   inline
   Serializer& write(const T& t)
   {
      return write(t, bool_<isPod<T>::value>());
   }

   template<typename T>
   inline
   Serializer& write(T t, tTrueType)
   {
      dbus_message_iter_append_basic(&iter_, dbus_type_code<T>::value, &t);
      return *this;
   }

   template<typename T>
   inline
   Serializer& write(const T& t, tFalseType)
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

   template<typename T>
   inline
   Serializer& write(const std::vector<T>& v)
   {/*
      int32_t len = v.size();
      enlarge(len*sizeof(T) + sizeof(len));

      memcpy(current_, &len, sizeof(len));
      current_ += sizeof(len);

      std::for_each(v.begin(), v.end(), VectorSerializer<Serializer>(*this));
*/
      return *this;
   }

   template<typename KeyT, typename ValueT>
   Serializer& write(const std::map<KeyT, ValueT>& m)
   {
       /*
      int32_t len = m.size();
      enlarge(len*(sizeof(KeyT) + sizeof(ValueT)) + sizeof(len));

      memcpy(current_, &len, sizeof(len));
      current_ += sizeof(len);

      std::for_each(m.begin(), m.end(), std::bind(&Serializer::write<KeyT, ValueT>, this, std::placeholders::_1));
*/
      return *this;
   }

   template<typename... T>
   Serializer& write(const std::tuple<T...>& t)
   {
      std_tuple_for_each(t, TupleSerializer<Serializer>(*this));
      return *this;
   }

   template<typename T1, typename T2>
   Serializer& write(const SerializerTuple<T1, T2>& tuple)
   {
      tuple.write(*this);
      return *this;
   }


private:

   template<typename KeyT, typename ValueT>
   inline
   void write(const std::pair<KeyT, ValueT>& p)
   {
      write(p.first).write(p.second);
   }

public:   // FIXME make friend?!

   DBusMessageIter iter_;
   DBusMessageIter* parent_;
};

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#define MAKE_SERIALIZER(type) \
inline \
simppl::ipc::detail::Serializer& operator<<(simppl::ipc::detail::Serializer& s, type t) \
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


// forward decl
template<typename VectorT>
simppl::ipc::detail::Serializer& operator<<(simppl::ipc::detail::Serializer&, const simppl::ipc::ServerVectorAttributeUpdate<VectorT>&);


inline
simppl::ipc::detail::Serializer& operator<<(simppl::ipc::detail::Serializer& s, const std::string& str)
{
   return s.write(str);
}


template<typename T>
inline
simppl::ipc::detail::Serializer& operator<<(simppl::ipc::detail::Serializer& s, const std::vector<T>& v)
{
   return s.write(v);
}


template<typename KeyT, typename ValueT>
inline
simppl::ipc::detail::Serializer& operator<<(simppl::ipc::detail::Serializer& s, const std::map<KeyT, ValueT>& m)
{
   return s.write(m);
}


template<typename StructT>
inline
simppl::ipc::detail::Serializer& operator<<(simppl::ipc::detail::Serializer& s, const StructT& st)
{
   return s.write(st);
}


template<typename... T>
inline
simppl::ipc::detail::Serializer& operator<<(simppl::ipc::detail::Serializer& s, const std::tuple<T...>& t)
{
   return s.write(t);
}


template<typename SerializerT>
template<typename T>
inline
void simppl::ipc::detail::TupleSerializer<SerializerT>::operator()(const T& t)   // seems to be already a reference so no copy is done
{
  s_ << t;
}


// -----------------------------------------------------------------------------


namespace simppl
{

namespace ipc
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

   ~Deserializer();

   template<typename T>
   inline
   Deserializer& read(T& t)
   {
      return read(t, bool_<isPod<T>::value>());
   }

   template<typename T>
   Deserializer& read(T& t, tTrueType)
   {
      dbus_message_iter_get_basic(&iter_, &t);
      return *this;
   }

   template<typename T>
   Deserializer& read(T& t, tFalseType)
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

   template<typename T>
   Deserializer& read(std::vector<T>& v)
   {
      uint32_t len;
      read(len);

      if (len > 0)
      {
         v.resize(len);

         for(uint32_t i=0; i<len; ++i)
         {
            read(v[i]);
         }
      }
      else
         v.clear();

      return *this;
   }

   template<typename KeyT, typename ValueT>
   Deserializer& read(std::map<KeyT, ValueT>& m)
   {
      uint32_t len;
      read(len);

      if (len > 0)
      {
         for(uint32_t i=0; i<len; ++i)
         {
            std::pair<KeyT, ValueT> p;
            read(p.first).read(p.second);
            m.insert(p);
         }
      }
      else
         m.clear();

      return *this;
   }

   template<typename... T>
   Deserializer& read(std::tuple<T...>& t)
   {
      std_tuple_for_each(t, TupleDeserializer<Deserializer>(*this));
      return *this;
   }

   template<typename T1, typename T2>
   Deserializer& read(SerializerTuple<T1, T2>& tuple)
   {
      tuple.read(*this);
      return *this;
   }

private:

   static inline
   char* allocate(size_t len)
   {
      return new char[len];
   }

public: // FIXME private?

   DBusMessageIter iter_;
   DBusMessageIter* parent_;
};

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#define MAKE_DESERIALIZER(type) \
inline \
simppl::ipc::detail::Deserializer& operator>>(simppl::ipc::detail::Deserializer& s, type& t) \
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


// forward decl
template<typename VectorT>
simppl::ipc::detail::Deserializer& operator>>(simppl::ipc::detail::Deserializer&, simppl::ipc::ClientVectorAttributeUpdate<VectorT>&);


inline
simppl::ipc::detail::Deserializer& operator>>(simppl::ipc::detail::Deserializer& s, std::string& str)
{
   return s.read(str);
}

template<typename T>
inline
simppl::ipc::detail::Deserializer& operator>>(simppl::ipc::detail::Deserializer& s, std::vector<T>& v)
{
   return s.read(v);
}

template<typename KeyT, typename ValueT>
inline
simppl::ipc::detail::Deserializer& operator>>(simppl::ipc::detail::Deserializer& s, std::map<KeyT, ValueT>& m)
{
   return s.read(m);
}

template<typename StructT>
inline
simppl::ipc::detail::Deserializer& operator>>(simppl::ipc::detail::Deserializer& s, StructT& st)
{
   return s.read(st);
}

template<typename... T>
inline
simppl::ipc::detail::Deserializer& operator>>(simppl::ipc::detail::Deserializer& s, std::tuple<T...>& t)
{
   return s.read(t);
}


template<typename DeserializerT>
template<typename T>
inline
void simppl::ipc::detail::TupleDeserializer<DeserializerT>::operator()(T& t)
{
   s_ >> t;
}


// ------------------------------------------------------------------------


namespace simppl
{

namespace ipc
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
Serializer& serialize(Serializer& s, T1 t1, T... t)
{
   s << t1;
   return serialize(s, t...);
}


// -----------------------------------------------------------------------


// FIXME use calltraits or rvalue references here

template<int N, typename TupleT>
struct FunctionCaller
{
   template<typename FunctorT>
   static inline
   void eval(FunctorT& f, const TupleT& tuple)
   {
      FunctionCaller<N, TupleT>::template eval_intern(f, tuple);
   }

   template<typename FunctorT>
   static inline
   void eval_cs(FunctorT& f, const CallState& cs, const TupleT& tuple)
   {
      FunctionCaller<N, TupleT>::template eval_intern_cs(f, cs, tuple);
   }

   template<typename FunctorT, typename... T>
   static inline
   void eval_intern(FunctorT& f, const TupleT& tuple, const T&... t)
   {
     FunctionCaller<N+1 == std::tuple_size<TupleT>::value ? -1 : N+1, TupleT>::template eval_intern(f, tuple, t..., std::get<N>(tuple));
   }

   template<typename FunctorT, typename... T>
   static inline
   void eval_intern_cs(FunctorT& f, const CallState& cs, const TupleT& tuple, const T&... t)
   {
      FunctionCaller<N+1 == std::tuple_size<TupleT>::value ? -1 : N+1, TupleT>::template eval_intern_cs(f, cs, tuple, t..., std::get<N>(tuple));
   }
};

template<typename TupleT>
struct FunctionCaller<-1, TupleT>
{
   template<typename FunctorT, typename... T>
   static inline
   void eval_intern(FunctorT& f, const TupleT& /*tuple*/, const T&... t)
   {
      f(t...);
   }

   template<typename FunctorT, typename... T>
   static inline
   void eval_intern_cs(FunctorT& f, const CallState& cs, const TupleT& /*tuple*/, const T&... t)
   {
      f(cs, t...);
   }
};


template<typename... T>
struct DeserializeAndCall : simppl::NonInstantiable
{
   template<typename FunctorT>
   static inline
   void eval(Deserializer& d, FunctorT& f)
   {
      std::tuple<T...> tuple;
      d >> tuple;

      FunctionCaller<0, std::tuple<T...>>::template eval(f, tuple);
   }

   template<typename FunctorT>
   static inline
   void evalResponse(Deserializer& d, FunctorT& f, const simppl::ipc::CallState& cs)
   {
      std::tuple<T...> tuple;

      if (cs)
         d >> tuple;

      FunctionCaller<0, std::tuple<T...>>::template eval_cs(f, cs, tuple);
   }
};


struct DeserializeAndCall0 : simppl::NonInstantiable
{
   template<typename FunctorT>
   static inline
   void eval(Deserializer& /*d*/, FunctorT& f)
   {
      f();
   }

   template<typename FunctorT>
   static inline
   void evalResponse(Deserializer& /*d*/, FunctorT& f, const simppl::ipc::CallState& cs)
   {
      f(cs);
   }
};


template<typename... T>
struct GetCaller : simppl::NonInstantiable
{
   typedef typename if_<sizeof...(T) == 0, DeserializeAndCall0, DeserializeAndCall<T...>>::type type;
};

}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERIALIZATION_H
