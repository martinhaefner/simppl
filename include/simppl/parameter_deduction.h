#ifndef SIMPPL_PARAMETERDEDUCTION_H
#define SIMPPL_PARAMETERDEDUCTION_H


namespace simppl
{
   
namespace dbus
{
   

struct Oneway 
{
   typedef Oneway real_type;
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


#include "simppl/parameter_deduction.h"


}   // namespace dbus

}   // namespace simppl

