#ifndef SIMPPL_CALCULATOR_H
#define SIMPPL_CALCULATOR_H


//#include "simppl/stub.h"
//#include "simppl/skeleton.h"
#include "simppl/interface.h"


/// servicebroker
INTERFACE(Calculator)
{   
   Request<> clear;
   
   Request<double> add;
   Request<double> sub;
   
   Attribute<double, Always> value;

   inline
   Calculator()
    : INIT_REQUEST(clear)
    , INIT_REQUEST(add)
    , INIT_REQUEST(sub)
    , INIT_ATTRIBUTE(value)
   {
      // NOOP
   }
};


#endif   // SIMPPL_CALCULATOR_H
