#ifndef SIMPPL_PROPERTY_H
#define SIMPPL_PROPERTY_H


#include "simppl/variant.h"


namespace simppl
{
   
namespace dbus
{

enum PropertyFlags
{
   ReadOnly     = 0,
   ReadWrite    = (1<<0),
   Notifying    = (1<<1)
};


namespace detail
{
   
template<typename T>
inline
void serialize_property(Serializer& s, const T& t)
{
   VariantSerializer<Serializer> vs(s);
   vs(t);
}

}   // detail

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_PROPERTY_H
