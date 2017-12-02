#ifndef SIMPPL_ECHOSERVICE_H
#define SIMPPL_ECHOSERVICE_H

#include <string>

#include <stdio.h>
#include "simppl/interface.h"


namespace simppl
{

namespace example 
{
    using namespace simppl::dbus;


    INTERFACE(EchoService)
    {
        Method<in<std::string>, out<std::string>> echo;

        // constructor
        EchoService()
         : INIT(echo)
        {
           // NOOP
        }
    };
    
}   // namespace example

}   // namespace simppl


#endif //SIMPPL_ECHOSERVICE_H
