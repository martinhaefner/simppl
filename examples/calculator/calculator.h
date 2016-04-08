#ifndef SIMPPL_CALCULATOR_H
#define SIMPPL_CALCULATOR_H


#include "simppl/interface.h"


namespace org
{

namespace simppl
{

INTERFACE(Calculator)
{
   Request<> clear;

   Request<double> add;
   Request<double> sub;

   Attribute<double> value;

   inline
   Calculator()
    : INIT(clear)
    , INIT(add)
    , INIT(sub)
    , INIT(value)
   {
      // NOOP
   }
};

}   // namespace simppl

}   // namespace org


#endif   // SIMPPL_CALCULATOR_H
