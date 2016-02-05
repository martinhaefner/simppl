#include "simppl/detail/util.h"

#include <cstring>


namespace simppl
{
   
namespace ipc
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


}   // namespace detail

}   // namespace ipc

}   // namespace simppl
