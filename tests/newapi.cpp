#include "simppl/ClientRequest.h"


int main()
{
   // one out
   {
      typedef Request<in<int>, in<double>, out<std::string>> request_type;
      static_assert(std::is_same<request_type::return_type, std::string>::value, "hey, not same");
      static_assert(request_type::is_oneway == false, "shall NOT be oneway");
      
      request_type r;
      std::string rc = r(42, 3.1415);
      r.async.f_(42, "Hallo");
   }
   
   // one out, different order
   {
      typedef Request<in<int>, out<std::string>, in<double>> request_type;
      static_assert(std::is_same<request_type::return_type, std::string>::value, "hey, not same");
      
      request_type r;
      std::string rc = r(42, 3.1415);
   }
   
   // no out at all
   {
      typedef Request<in<int>, in<double>, Oneway> request_type;
      static_assert(std::is_same<request_type::return_type, void>::value, "hey, not same");
      static_assert(request_type::is_oneway == true, "shall be oneway");
      
      request_type r;
      r(42, 3.1415);
   }

   // multiple out
   {
      typedef Request<in<int>, in<double>, out<int>, out<std::string>> request_type;
      static_assert(std::is_same<request_type::return_type, std::tuple<int, std::string>>::value, "hey, not same");
      
      request_type r;
      std::tuple<int, std::string> rc = r(42, 3.1415);
      r.async.f_(42, 42, "Hallo");
   }
   
   // just one in 
   {
      typedef Request<in<int>> request_type;
      static_assert(std::is_same<request_type::return_type, void>::value, "hey, not same");
      
      request_type r;
      r(42);
      r.async.f_(42);
   }
   
   // no in 
   {
      typedef Request<out<int>> request_type;
      static_assert(std::is_same<request_type::return_type, int>::value, "hey, not same");
      
      request_type r;
      int i = r();
      r.async.f_(42, 42);
   }
   
   // no in, no out 
   {
      typedef Request<> request_type;
      static_assert(std::is_same<request_type::return_type, void>::value, "hey, not same");
      static_assert(request_type::is_oneway == false, "shall NOT be oneway");
      
      request_type r;
      r();
      r.async.f_(42);
   }

   // no in, no out 
   {
      typedef Request<Oneway> request_type;
      static_assert(std::is_same<request_type::return_type, void>::value, "hey, not same");
      static_assert(request_type::is_oneway == true, "shall be oneway");
      
      request_type r;
      r();
      r.async.f_(42);
   }
   
   // one in, one out 
   {
      typedef Request<in<int>, out<std::string>> request_type;
      static_assert(std::is_same<request_type::return_type, std::string>::value, "hey, not same");
      
      request_type r;
      std::string rc = r(42);
      r.async.f_(42, "Hallo");
      
      r.async(42);
   }

   // one in, one out 
   {
      typedef Request<out<std::string>, in<int>> request_type;
      static_assert(std::is_same<request_type::return_type, std::string>::value, "hey, not same");
      
      request_type r;
      std::string rc = r(42);
      r.async.f_(42, "Hallo");
   }
 
   return 0;
}
