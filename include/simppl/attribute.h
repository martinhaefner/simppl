#ifndef SIMPPL_ATTRIBUTE_H
#define SIMPPL_ATTRIBUTE_H


#include "simppl/detail/is_vector.h"


namespace simppl
{
   
namespace dbus
{

enum AttributeFlags
{
   ReadOnly     = 0,
   ReadWrite    = (1<<0),
   Notifying    = (1<<1)
};

}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_ATTRIBUTE_H
