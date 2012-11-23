#ifndef SIMPPL_VALIDATION_H
#define SIMPPL_VALIDATION_H


// forward decl
template<typename T>
struct isValidType;


template<typename T>
struct isValidTuple 
{ 
   enum { value = false }; 
};

// forward decl
template<typename... T>
struct isValidTuple<std::tuple<T...> >;

// recurse into variadic template arguments
template<typename T1, typename... T>
struct isValidTuple<std::tuple<T1, T...> >
{ 
   enum { value = isValidType<T1>::value && isValidTuple<std::tuple<T...> >::value };
};

template<typename T>
struct isValidTuple<std::tuple<T> >
{ 
   enum { value = isValidType<T>::value };
};


// --------------------------------------------------------------------------------------------


template<typename T>
struct isValidSerializerTuple
{ 
   enum { value = false }; 
};

template<typename T1, typename T2>
struct isValidSerializerTuple<SerializerTuple<T1, T2> > 
{ 
   enum { value = isValidType<T1>::value && isValidType<T2>::value }; 
};

template<typename T>
struct isValidSerializerTuple<SerializerTuple<T, NilType> > 
{ 
   enum { value = isValidType<T>::value }; 
};


template<typename T, unsigned int>
struct TupleValidator;

template<typename T>
struct TupleValidator<T, 4>
{
   enum { value = isValidSerializerTuple<typename T::serializer_type>::value };
};

template<typename T>
struct TupleValidator<T, 1>
{
   enum { value = false };
};


template<typename T>
struct isValidStruct
{
   template<typename U>
   static int senseless(typename U::serializer_type*);
   
   template<typename U>
   static char senseless(...);
   
   enum { value = TupleValidator<T, sizeof(senseless<T>(0))>::value };
};


template<typename VectorT, unsigned int>
struct InternalVectorValidator;

template<typename VectorT>
struct InternalVectorValidator<VectorT, 4>
{
   enum { value = isValidType<typename VectorT::value_type>::value };
};

template<typename VectorT>
struct InternalVectorValidator<VectorT, 1>
{
   enum { value = false };
};


template<typename T>
struct isVector
{
   template<typename U>
   static int senseless(const std::vector<U>*);
   static char senseless(...);

   enum { value = InternalVectorValidator<T ,sizeof(senseless((const T*)0))>::value };
};


template<typename MapT, unsigned int>
struct InternalMapValidator;

template<typename MapT>
struct InternalMapValidator<MapT, 4>
{
   enum { value = isValidType<typename MapT::key_type>::value && isValidType<typename MapT::mapped_type>::value };
};

template<typename MapT>
struct InternalMapValidator<MapT, 1>
{
   enum { value = false };
};


template<typename T>
struct isMap
{
   template<typename U, typename V>
   static int senseless(const std::map<U, V>*);
   static char senseless(...);
   
   enum { value = InternalMapValidator<T, sizeof(senseless((const T*)0))>::value };
};


template<typename T>
struct isString
{
   enum { value = is_same<T, std::string>::value };
};


template<typename T>
struct isValidType
{
   enum { 
      value =
         isVoid<T>::value
      || isPod<T>::value 
      || isValidStruct<T>::value 
      || isVector<T>::value 
      || isMap<T>::value 
      || isString<T>::value
      || isValidTuple<T>::value
      || isValidSerializerTuple<T>::value
   };
};


#endif   // SIMPPL_VALIDATION_H
