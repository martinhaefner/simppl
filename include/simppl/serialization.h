#ifndef SIMPPL_SERIALIZATION_H
#define SIMPPL_SERIALIZATION_H


#include "simppl/detail/serialization.h"


namespace simppl
{
   
namespace ipc
{

template<typename... T>
struct make_serializer
{
   typedef typename make_typelist<T...>::type type__;
   typedef typename detail::make_serializer_imp<type__>::type type;
};

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_SERIALIZATION_H