#include <iostream>
#define EX 1
#include "include/inifile.h"
#ifdef EX
#include "include/inifile_extensionpoint.h"
#endif

#define __BINDER_NS ini

//#define HAVE_BOOST 1
//#define HAVE_TR1 1

#include "include/bind_adapter.h"


void funca(const char* text, int i)
{
   std::cout << text << ": " << i << std::endl;
}

bool funci(bool b)
{
   std::cout << "Hallo " << (b?"Welt":"Shit") << "!" << std::endl;
   return true;
}

bool b(const char* what)
{
   std::cout << "Gell, " << what << "?" << std::endl;
   return true;
}

bool f(const char* what)
{
   std::cout << "Super " << what << "!" << std::endl;
   return true;
}

bool local(const char* what)
{
   std::cout << "Local is '" << what << "'" << std::endl;
   return (strcmp(what, "super"));
}

bool forward(const char*)
{
   return true;
}


#ifdef EX
ini::ExtensionPoint extensionpoint1 = ini::Section("gurgel") >> f;

ini::ExtensionPoint extensionpoint2;
#endif


int main()
{
   const char* filename =  "./test.ini";
   std::vector<int> v;
   short bl = 0;
   bool b1 = false;
   unsigned int i2 = 0;
   char c = 0;
   char buf[255];
   std::string str1;

   ini::Result rc = ini::Parser<
         ini::NoopHeaderParser, 
         ini::ReportUnknownSection, 
         ini::ReportUnknownKey>
      ::parse(
         ini::File(filename, "/etc/test.ini")[
               ini::Key<>("one") >> __BINDER_NS::bind(&funca, "one", _1)
            || ini::Key<ini::Mandatory>("two") >> __BINDER_NS::bind(&funca, "two", _1)
            || ini::Key<>("three") >> __BINDER_NS::bind(&funca, "three", _1)
            || ini::Key<ini::Mandatory>("four") >> __BINDER_NS::bind(&funca, "four", _1)
         ]
         <= ini::Section("gaga")[
               ini::Key<>("hallo") >> v     
            || ini::Key<>("hehe") >> bl
            || ini::Key<>("gell") >> b1
            || ini::Key<>("gell1") >> i2 
            || ini::Key<>("gell4") >> c
            || ini::Key<>("gell2") >> __BINDER_NS::bind(strncpy, buf, _1, sizeof(buf))
            || ini::Key<>("gell5") >> buf
            || ini::Key<>("gell3") >> str1
         ]
         <= ini::Section("jepp") >> local  
#ifdef EX         
         <= extensionpoint1
         <= extensionpoint2
#endif
      );
   
   if (!rc)
      std::cout << "Failed to parse ini-File in line " << rc.line() << ": " << rc.toString() << ". Must exit." << std::endl;
   
   std::cout << bl << std::endl;
   
   return EXIT_SUCCESS;
}
