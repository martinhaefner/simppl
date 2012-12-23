#include "simppl/cmdline_extensionpoint.h"

#include <iostream>
#include <stdio.h>

#include "simppl/bind_adapter.h"

using std::placeholders::_1;

namespace cmd = simppl::cmdline;


//#define USE_GETOPT_PARSER


// -----------------------------------------------------------------------------------------------------


bool gFlag = false;


void f()
{
   gFlag = true;
}


void g(int value)
{
   std::cout << "j says " << value << std::endl;
}


struct Nix
{
   void sayHello(int i)
   {
      std::cout << i << std::endl;
   }
};

Nix n;

std::string theOption;

void g1(const std::string& value)
{
   theOption = value;
   std::cout << "theOption=" << theOption << std::endl;
}


cmd::ExtensionPoint globalExtensionPoint = 
      cmd::Option<cmd::NoChar>()["jiji1"].doc("Tolle Show, hier!") >> g1
   <= cmd::Switch<cmd::NoChar>()["kurios"] >> simppl::bind(&Nix::sayHello, n, 42)
   <= cmd::Option<cmd::NoChar>()["jiji2"].doc("Tolle Show, hier123!") >> g
   ;

cmd::ExtensionPoint secondExtensionPoint = 
      cmd::Option<cmd::NoChar>()["jiji4"].doc("Tolle Show, hier!") >> g
   <= cmd::Option<cmd::NoChar>()["jiji5"].doc("Tolle Show, hier123!") >> g
   ;

// yet another argument to be added here
bool registerExtension()
{
   globalExtensionPoint += cmd::Option<cmd::NoChar>()["jiji3"].doc("Noch ne tolle Show, hier!") >> g;
}

static bool registered = registerExtension();


struct A
{
   A(bool& flag)
    : flag_(flag)
   {
   }
   
   void hello(bool value)
   {
      flag_ = value;
   }
   
   void store(int value)
   {
      flag_ = value > 0;
   }

   void store1(const char* what)
   {
      printf("cmdline says '%s'\n", what);
   }

   void store2(const char* what)
   {
      printf("now it says '%s'\n", what);
   }
         
   bool& flag_;
};


void h(int j)
{
   printf("from z: %d\n", j);
}


struct MyUsage : /*Noop*/cmd::DefaultUsagePrinter
{
   inline
   void operator()(std::ostream& os)
   {
      os << "This is an absolutely senseless program." << std::endl;
   }
   
   inline
   void operator()(FILE* f)
   {
      fprintf(f, "This is an absolutely senseless program.\n");
   }
};


int main(int argc, char** argv)
{
   A a(gFlag);
   int i;
   int k;
   const char* ptr = 0;
   char what[255] = { 0 };
   std::string strng;
   std::vector<std::string> vec;
     
   std::function<void(int)> func(h);
   
#ifdef USE_GETOPT_PARSER
   
   bool rc = true;
   
   int c;
   bool have_z = false;
   
   while((c = getopt(argc, argv, "z:yx:v:w:u:")) != -1)
   {
      switch(c)
      {
         case 'z':
            {
               int i=0;
               if ((i = atoi(optarg)) != 0)
               {
                  have_z = true;
                  h(i);
               }
               else
                  rc = false;
            }
            break;
            
         case 'y':
            gFlag = true;
            break;
            
         case 'x':
            {
               int i=0;
               if ((i = atoi(optarg)) != 0)
               {
                  k = i;
               }
               else
                  rc = false;
            }
            break;
            
         case 'v':
            ptr = optarg;
            break;
            
         case 'w':
            strcpy(what, optarg);
            break;
            
         case 'u':
            strng = optarg;
            break;
            
         default:
            rc = false;
            break;
      }
   }
   
   if (rc && optind == argc-1)
   {
      a.store1(argv[optind]);
   }
   
#else
//#   define _ERASE(a) 
#   define _ERASE(a) a 
#if 0
   bool rc = cmd::Parser<cmd::IgnoreUnknown, cmd::LongOptionSupport, cmd::DefaultUsagePrinter/*MyUsage*/>
      ::parse(argc, argv,
              cmd::Switch<'v'>().doc("super") >> cmd::inc(k) 
              <= globalExtensionPoint 
              /*<= Switch<'w'>().doc("super") >> inc(k) 
              <= secondExtensionPoint
              <= cmd::Switch<'x'>().doc("super") >> inc(k)*/
             );
#endif
  // #if 0
   bool rc = cmd::Parser<cmd::IgnoreUnknown, cmd::LongOptionSupport, /*cmd::NoopUsagePrinter*/MyUsage>
      ::parse(argc, argv,
              //cmd::Switch<'v'>().doc("super") >> cmd::inc(k) ||
              //cmd::MultiOption<>()["bp"].doc("toll") >> h
              
              cmd::Option<cmd::NoChar, cmd::Mandatory>()_ERASE(["gaga"].doc("Hallo welt")) >> h 
              <= cmd::Switch<'y'>() >> gFlag 
              <= cmd::Switch<>()["gagaga"].doc("super") >> gFlag 
              <= cmd::Option<'x'>()_ERASE(["haha"].doc("Super Ding fuer int")) >> k                // integer
              <= cmd::Option<'v'>()_ERASE(["hehe"].doc("Super Ding fuer ptr")) >> &ptr             // const char ptrs
              <= cmd::Option<'w'>()_ERASE(["hihi"].doc("Super Ding fuer char arrays")) >> what     // char arrays
              <= cmd::Option<'u'>()_ERASE(["huhu"].doc("Super Ding fuer std::string")) >> strng    // std::strings
              <= cmd::ExtraOption<cmd::Mandatory>()_ERASE(.doc("tolles Argument, echt super")) >> simppl::bind(&A::store1, &a, _1) 
              //ExtraOption<Mandatory>()_ERASE(.doc("tolles Argument, echt super")) >> bind(&std::vector<std::string>::push_back, &vec, _1) 
              <= cmd::MultiExtraOption<cmd::Mandatory>()_ERASE(.doc("tolles Argument, echt super")) >> vec 
              //cmd::ExtraOption<Mandatory>()_ERASE(.doc("tolles Argument, echt super")) >> what 
              //cmd::ExtraOption<>()_ERASE(.doc("tolles Argument, ehrlich, wirklich")) >> bind(&A::store2, &a, _1)
              <= globalExtensionPoint
              
             );
//#endif
#endif
   
   if (rc)
   {
      printf("verbosity: %d\n", k);
      
      printf("y: %s set\n", gFlag?"is":"not");
      
      if (ptr)
         printf("v: %s\n", ptr);
         
      if (what[0])
         printf("w: %s\n", what);
      
      if (vec.size() > 0)
      {
         printf("vec: ");
         for (int i=0; i<vec.size()-1; ++i)
         {
            printf("%s, ", vec[i].c_str());
         }
         printf("%s\n", vec[vec.size()-1].c_str());
      }
   }
   else
      printf("Must exit.\n");
      
   return 0;
}
