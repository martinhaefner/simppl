#include "simppl/stub.h"
#include "simppl/interface.h"


namespace spl = simppl::ipc;


INTERFACE(Simple)
{
   Request<int> echo;
   Signal<double> sigUsr;
   Response<int> rEcho;
   
   Simple()
    : INIT_REQUEST(echo)
    , INIT_SIGNAL(sigUsr)
    , INIT_RESPONSE(rEcho)
   {
      echo >> rEcho;
   }
};


int main()
{
   spl::Stub<Simple> sst("my", "simppl.simple");
   
   return 0;
}
