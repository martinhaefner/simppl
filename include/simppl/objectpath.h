#ifndef SIMPPL_OBJECTPATH_H
#define SIMPPL_OBJECTPATH_H


#include <string>

#include "simppl/serialization.h"


namespace simppl
{

namespace dbus
{


struct ObjectPath
{
    /**
     * Empty or invalid object paths may not be sent via DBus according to
     * the specification.
     */
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

    inline friend bool operator==(const ObjectPath& lhs, const ObjectPath& rhs)
    {
        return lhs.path == rhs.path;
    }

    inline friend bool operator!=(const ObjectPath& lhs, const ObjectPath& rhs)
    {
        return !(lhs == rhs);
    }

    inline friend bool operator<(const ObjectPath& lhs, const ObjectPath& rhs)
    {
        return lhs.path < rhs.path;
    }

    inline friend bool operator<=(const ObjectPath& lhs, const ObjectPath& rhs)
    {
        return !(lhs > rhs);
    }

    inline friend bool operator>(const ObjectPath& lhs, const ObjectPath& rhs)
    {
        return rhs < lhs;
    }

    inline friend bool operator>=(const ObjectPath& lhs, const ObjectPath& rhs)
    {
        return !(lhs < rhs);
    }
    
    std::string path;
};


struct ObjectPathCodec : composite_signature<signature_chars<DBUS_TYPE_OBJECT_PATH>>
{
   static
   void encode(Encoder& e, const ObjectPath& p);

   static
   void decode(Decoder& d, ObjectPath& p);
};


template<>
struct Codec<ObjectPath> : public ObjectPathCodec {};


}   // dbus

}   // simppl


#endif   // SIMPPL_OBJECTPATH_H
