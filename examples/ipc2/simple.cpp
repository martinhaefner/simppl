#include <simppl/Stub.h>


INTERFACE(Simple)
{
   Request<int> echo;
   //Response<int> rEcho;
   
   Simple()
    : INIT_REQUEST(echo)
    //, INIT_RESPONSE(rEcho)
   {
      // echo >> rEcho;
   }
}


int main()
{
   Stub<Simple> sst("my", "simppl.simple");
   
   return 0;
}
