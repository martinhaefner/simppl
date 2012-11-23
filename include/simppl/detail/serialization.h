#ifndef SIMPPL_SERIALIZATION_H
#define SIMPPL_SERIALIZATION_H


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
    : s_(s)
   {
      // NOOP
   }
   
   template<typename T>
   void operator()(const T& t)   // seems to be already a reference so no copy is done
   {
      s_.write(t);
   }
   
   SerializerT& s_;
};


template<typename DeserializerT>
struct TupleDeserializer // : noncopable
{
   TupleDeserializer(DeserializerT& s)
    : s_(s)
   {
      // NOOP
   }
   
   template<typename T>
   void operator()(T& t)
   {
      s_.read(t);
   }
   
   DeserializerT& s_;
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

template<typename... T>
struct make_serializer
{
   typedef typename make_typelist<T...>::type type__;
   typedef typename make_serializer_imp<type__>::type type;
};


// ------------------------------------------------------------------------------------


template<size_t N>
struct StdTupleForEach
{
   template<typename TupleT, typename FunctorT>
   static inline
   void eval(TupleT& t, FunctorT func)
   {
      func(std::get<N>(t));
      StdTupleForEach<N-1>::template eval(t, func);
   }
};

template<>
struct StdTupleForEach<0>
{
   template<typename TupleT, typename FunctorT>
   static inline
   void eval(TupleT& t, FunctorT func)
   {
      func(std::get<0>(t));
   }
};


template<typename TupleT, typename FunctorT>
inline
void std_tuple_for_each(TupleT& t, FunctorT functor)
{
   StdTupleForEach<std::tuple_size<typename std::remove_const<TupleT>::type>::value-1>::template eval(t, functor);
}


// ------------------------------------------------------------------------------------


struct Serializer // : noncopyable
{   
   inline
   Serializer(size_t initial = 64)
    : capacity_(initial)
    , buf_(capacity_ > 0 ? (char*)malloc(capacity_) : 0)
    , current_(buf_)
   {
      // NOOP
   }

   inline
   ~Serializer()
   {
      free(buf_);
   }
   
   static inline
   void free(void* ptr)
   {
      return ::free(ptr);
   }
 
   /// any subsequent calls to the object are invalid
   inline
   void* release()
   {
      void* rc = buf_;
      buf_ = 0;
      return rc;
   }
   
   inline
   size_t size() const
   {
      if (capacity_ > 0)
      {
         assert(buf_);
         return current_ - buf_;
      }
      else
         return 0;
   }
   
   inline
   const void* data() const
   {
      return buf_;
   }
   
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
      enlarge(sizeof(T));
      
      memcpy(current_, &t, sizeof(T));
      current_ += sizeof(T);
      
      return *this;
   }
   
   template<typename T>
   inline
   Serializer& write(const T& t, tFalseType)
   {
      return write(*(const typename T::serializer_type*)&t);
   }
   
   Serializer& write(const std::string& str)
   {
      int32_t len = str.size();
      enlarge(len + sizeof(len));
      
      memcpy(current_, &len, sizeof(len));
      current_ += sizeof(len);
      memcpy(current_, str.c_str(), len);
      current_ += len;
      
      return *this;
   }
   
   template<typename T>
   inline
   Serializer& write(const std::vector<T>& v)
   {
      int32_t len = v.size();
      enlarge(len*sizeof(T) + sizeof(len));
 
      memcpy(current_, &len, sizeof(len));
      current_ += sizeof(len);
 
      std::for_each(v.begin(), v.end(), VectorSerializer<Serializer>(*this));
            
      return *this;
   }
   
   template<typename KeyT, typename ValueT>
   Serializer& write(const std::map<KeyT, ValueT>& m)
   {
      int32_t len = m.size();
      enlarge(len*(sizeof(KeyT) + sizeof(ValueT)) + sizeof(len));
      
      memcpy(current_, &len, sizeof(len));
      current_ += sizeof(len);
 
      std::for_each(m.begin(), m.end(), std::bind(&Serializer::write<KeyT, ValueT>, this, std::placeholders::_1));

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
   
   void enlarge(size_t needed)
   {
      size_t current = size();
      size_t estimated_capacity = current + needed;
      
      if (capacity_ < estimated_capacity)
      {
         while (capacity_ < estimated_capacity)
            capacity_ <<=1 ;
         
         buf_ = (char*)realloc(buf_, capacity_);
         current_ = buf_ + current;
      }
   }
   
   size_t capacity_;
   
   char* buf_;
   char* current_;
};


#define MAKE_SERIALIZER(type) \
inline \
Serializer& operator<<(Serializer& s, type t) \
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


inline 
Serializer& operator<<(Serializer& s, const std::string& str) 
{ 
   return s.write(str); 
}

template<typename T>
inline 
Serializer& operator<<(Serializer& s, const std::vector<T>& v) 
{ 
   return s.write(v); 
}

template<typename KeyT, typename ValueT>
inline 
Serializer& operator<<(Serializer& s, const std::map<KeyT, ValueT>& m) 
{ 
   return s.write(m); 
}

template<typename StructT>
inline
Serializer& operator<<(Serializer& s, const StructT& st)
{
   const typename StructT::serializer_type& tuple = *(const typename StructT::serializer_type*)&st;
   return s.write(tuple);
}

template<typename... T>
inline
Serializer& operator<<(Serializer& s, const std::tuple<T...>& t)
{
   return s.write(t);
}


// -----------------------------------------------------------------------------


struct Deserializer // : noncopyable
{
   static inline
   void free(char* ptr)
   {
      delete[] ptr;
   }
   
   inline
   Deserializer(const void* buf, size_t capacity)
    : capacity_(capacity)
    , buf_((const char*)buf)
    , current_(buf_)
   {
      // NOOP
   }
   
   inline
   size_t size() const
   {
      return current_ - buf_;
   }
   
   template<typename T>
   inline
   Deserializer& read(T& t)
   {
      return read(t, bool_<isPod<T>::value>());
   }
   
   template<typename T>
   Deserializer& read(T& t, tTrueType)
   {
      memcpy(&t, current_, sizeof(T));
      current_ += sizeof(T);
      
      return *this;
   }
   
   template<typename T>
   Deserializer& read(T& t, tFalseType)
   {
      return read(*(typename T::serializer_type*)&t);
   }
   
   Deserializer& read(char*& str)
   {
      assert(str == 0);   // we allocate the string via Deserializer::alloc -> free with Deserializer::free
      
      uint32_t len;
      read(len);
      
      if (len > 0)
      {
         str = allocate(len);
         memcpy(str, current_, len);
         current_ += len;
      }
      else
      {
         str = allocate(1);
         *str = '\0';
      }
      
      return *this;
   }
   
   Deserializer& read(std::string& str)
   {
      uint32_t len;
      read(len);
      
      if (len > 0)
      {
         str.assign(current_, len);
         current_ += len;
      }
      else
         str.clear();
   
      return *this;
   }
   
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
   
   const size_t capacity_;
   
   const char* buf_;
   const char* current_;
};


#define MAKE_DESERIALIZER(type) \
inline \
Deserializer& operator>>(Deserializer& s, type& t) \
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


inline 
Deserializer& operator>>(Deserializer& s, std::string& str) 
{ 
   return s.read(str); 
}

template<typename T>
inline 
Deserializer& operator>>(Deserializer& s, std::vector<T>& v) 
{ 
   return s.read(v); 
}

template<typename KeyT, typename ValueT>
inline 
Deserializer& operator>>(Deserializer& s, std::map<KeyT, ValueT>& m) 
{ 
   return s.read(m); 
}

template<typename StructT>
inline
Deserializer& operator>>(Deserializer& s, StructT& st)
{
   typename StructT::serializer_type& tuple = *(typename StructT::serializer_type*)&st;
   return s.read(tuple);
}

template<typename... T>
inline
Deserializer& operator>>(Deserializer& s, std::tuple<T...>& t)
{
   return s.read(t);
}


#endif   // SIMPPL_SERIALIZATION_H
