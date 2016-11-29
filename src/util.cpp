#include "simppl/detail/util.h"

#include <cstring>
#include <cassert>
#include <cstdio>


namespace simppl
{

namespace dbus
{

namespace detail
{


char* create_objectpath(const char* iface, const char* role)
{
   size_t capacity = strlen(role) + strlen(iface) + 3;

   char* objectpath = new char[capacity];
   sprintf(objectpath, "/%s/%s", iface, role);
   char* p = objectpath;

   while(*p)
   {
      if (*p == '.')
         *p = '/';
         ++p;
   }

   return objectpath;
}


char* create_busname(const char* iface, const char* role)
{
   size_t capacity = strlen(role) + strlen(iface) + 2;

   char* busname = new char[capacity];
   sprintf(busname, "%s.%s", iface, role);

   return busname;
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
