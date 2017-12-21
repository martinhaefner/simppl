#ifndef SIMPPL_OBJECTPATH_H
#define SIMPPL_OBJECTPATH_H


#include <string>


namespace simppl
{

namespace dbus
{


struct ObjectPath
{
    inline
    ObjectPath()
    {
        // NOOP
    }

    inline
    ObjectPath(const std::string& p)
     : path(p)
    {
        // NOOP
    }
    
    inline
    ObjectPath(const char* p)
     : path(p)
    {
        // NOOP
    }

    inline
    bool operator<(const ObjectPath& rhs) const
    {
        return path < rhs.path;
    }

    std::string path;
};


namespace detail
{


template<>
struct Codec<ObjectPath>
{
   static 
   void encode(Serializer& s, const ObjectPath& p)
   {
      // FIXME
   }
   
   
   static 
   void decode(Deserializer& s, ObjectPath& p)
   {
      // FIXME
   }
};


template<>
struct make_type_signature<ObjectPath>
{
   static inline
   std::ostream& eval(std::ostream& os)
   {
      return os << DBUS_TYPE_OBJECT_PATH_AS_STRING;
   }
};


template<> struct dbus_type_code<simppl::dbus::ObjectPath> { enum { value = DBUS_TYPE_OBJECT_PATH }; };


}

}   // simppl


#endif   // SIMPPL_OBJECTPATH_H
