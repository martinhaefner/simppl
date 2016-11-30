#ifndef SIMPPL_PARAMETERDEDUCTION_H
#define SIMPPL_PARAMETERDEDUCTION_H


namespace simppl
{

namespace dbus
{


struct oneway
{
   typedef oneway real_type;
};


// marker
template<typename T>
struct in
{
   typedef T real_type;
};

// marker
template<typename T>
struct out
{
   typedef T real_type;
};


}   // namespace dbus

}   // namespace simppl


#include "simppl/detail/parameter_deduction.h"

#endif   // SIMPPL_PARAMETERDEDUCTION_H
