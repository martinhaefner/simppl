#include "simppl/detail/util.h"

#include <cstring>
#include <cassert>
#include <iostream>   // FIXME remove this

namespace simppl
{
   
namespace dbus
{

namespace detail
{


std::tuple<char*, char*> create_objectpath(const char* iface, const char* role)
{
   size_t capacity = strlen(role) + strlen(iface) + 3;
   
   char* objectpath = new char[capacity];
   sprintf(objectpath, "/%s/%s", iface, role);
   
   char* p = objectpath;
   
   while(*(++p))
   {
      if (*p == '.')
         *p = '/';
   }    
   
   return std::make_tuple(objectpath, objectpath + strlen(objectpath) - strlen(role));
}


char* extract_interface(const char* mangled_iface)
{
   assert(mangled_iface);
   
   size_t len = strstr(mangled_iface, "<") - mangled_iface;
   
   char* rc = new char[len+1];
   
   strncpy(rc, mangled_iface, len);
   rc[len] = '\0';
   
   // remove '::' separation in favour of '.' separation
   char *readp = rc, *writep = rc;
   while(*readp)
   {
      if (*readp == ':')
      {
         *writep++ = '.';
         readp += 2;
      }
      else
         *writep++ = *readp++;
   }

   // terminate
   *writep = '\0';
   
   return rc;
}


}   // namespace detail

}   // namespace dbus

}   // namespace simppl
