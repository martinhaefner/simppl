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

std::vector<std::string> extract_interfaces(std::size_t iface_count, const char* mangled_iface_list)
{
   assert(mangled_iface_list);
   assert(strcmp(mangled_iface_list, "simppl::make_typelist<") > 0);

   // The `mangled_iface` string will be on the form:
   //
   // simppl::make_typelist<
   //    ns1::interface1<Args...>,
   //    ns2::interface2<Args...>
   // >
   //
   // We are just interested in the "ns1::interface1" part.
   std::vector<std::string> interfaces;
   interfaces.reserve(iface_count);

   // skip the "simppl::make_typelist<" part
   const char* begin = mangled_iface_list + strlen("simppl::make_typelist<");

   do {
      const char* end = strchr(begin, '<');
      interfaces.push_back(make_interface_name(begin, end));
      begin = find_next_interface(end);
   } while (begin);

   return interfaces;
}

std::string make_interface_name(const char* src, const char* end)
{
   assert(end > src);

   std::string name(end - src, '\0');
   for (auto dst = name.begin(); src != end; ++dst, ++src) {
      if (*src == ':') {
         *dst = '.';
         ++src;
      } else {
         *dst = *src;
      }
   }
   return name;
}

const char* find_next_interface(const char* s) {
   assert(s && "input cannot be null");
   assert(*s == '<' && "expected '<'");

   ++s;
   int level = 1;

   for (; s && level > 0; ++s) {
      if (*s == '<') {
         ++level;
      } else if (*s == '>') {
         --level;
      }
   }

   assert(level == 0 && "unbalanced angle brackets");
   for (; isspace(*s); ++s)
      ;

   if (*s == ',') {
      for (++s; isspace(*s); ++s)
         ;
      return s;
   } else if (*s == '>') {
      return nullptr;
   } else {
      assert(false && "expected ',' or '>'");
      return nullptr;
   }
}

}   // namespace detail

}   // namespace dbus

}   // namespace simppl
