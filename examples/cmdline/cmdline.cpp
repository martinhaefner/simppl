#include "include/cmdline_extensionpoint.h"
#include "include/bind.h"

#include <iostream>
#include <stdio.h>

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


cmdline::ExtensionPoint globalExtensionPoint = 
      cmdline::Option<cmdline::NoChar>()["jiji1"].doc("Tolle Show, hier!") >> g1
   <= cmdline::Switch<cmdline::NoChar>()["kurios"] >> bind(&Nix::sayHello, n, 42)
   <= cmdline::Option<cmdline::NoChar>()["jiji2"].doc("Tolle Show, hier123!") >> g
   ;

cmdline::ExtensionPoint secondExtensionPoint = 
      cmdline::Option<cmdline::NoChar>()["jiji4"].doc("Tolle Show, hier!") >> g
   <= cmdline::Option<cmdline::NoChar>()["jiji5"].doc("Tolle Show, hier123!") >> g
   ;

// yet another argument to be added here
bool registerExtension()
{
   globalExtensionPoint += cmdline::Option<cmdline::NoChar>()["jiji3"].doc("Noch ne tolle Show, hier!") >> g;
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


struct MyUsage : /*Noop*/cmdline::DefaultUsagePrinter
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
     
   Function<void(*)(int)> func(h);
   
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
   bool rc = cmdline::Parser<cmdline::IgnoreUnknown, cmdline::LongOptionSupport, cmdline::DefaultUsagePrinter/*MyUsage*/>
      ::parse(argc, argv,
              cmdline::Switch<'v'>().doc("super") >> cmdline::inc(k) 
              <= globalExtensionPoint 
              /*<= Switch<'w'>().doc("super") >> inc(k) 
              <= secondExtensionPoint
              <= cmdline::Switch<'x'>().doc("super") >> inc(k)*/
             );
#endif
  // #if 0
   bool rc = cmdline::Parser<cmdline::IgnoreUnknown, cmdline::LongOptionSupport, cmdline::NoopUsagePrinter/*MyUsage*/>
      ::parse(argc, argv,
              //cmdline::Switch<'v'>().doc("super") >> cmdline::inc(k) ||
              //cmdline::MultiOption<>()["bp"].doc("toll") >> h
              
              cmdline::Option<cmdline::NoChar, cmdline::Mandatory>()_ERASE(["gaga"].doc("Hallo welt")) >> h 
              <= cmdline::Switch<'y'>() >> gFlag 
              <= cmdline::Switch<>()["gagaga"].doc("super") >> gFlag 
              <= cmdline::Option<'x'>()_ERASE(["haha"].doc("Super Ding fuer int")) >> k                // integer
              <= cmdline::Option<'v'>()_ERASE(["hehe"].doc("Super Ding fuer ptr")) >> &ptr             // const char ptrs
              <= cmdline::Option<'w'>()_ERASE(["hihi"].doc("Super Ding fuer char arrays")) >> what     // char arrays
              <= cmdline::Option<'u'>()_ERASE(["huhu"].doc("Super Ding fuer std::string")) >> strng    // std::strings
              <= cmdline::ExtraOption<cmdline::Mandatory>()_ERASE(.doc("tolles Argument, echt super")) >> bind(&A::store1, &a, _1) 
              //ExtraOption<Mandatory>()_ERASE(.doc("tolles Argument, echt super")) >> bind(&std::vector<std::string>::push_back, &vec, _1) 
              <= cmdline::MultiExtraOption<cmdline::Mandatory>()_ERASE(.doc("tolles Argument, echt super")) >> vec 
              //cmdline::ExtraOption<Mandatory>()_ERASE(.doc("tolles Argument, echt super")) >> what 
              //cmdline::ExtraOption<>()_ERASE(.doc("tolles Argument, ehrlich, wirklich")) >> bind(&A::store2, &a, _1)
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
