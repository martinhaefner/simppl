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

    inline
    bool operator!=(const ObjectPath& rhs) const
    {
        return path != rhs.path;
    }

    std::string path;
};


struct ObjectPathCodec
{
   static
   void encode(DBusMessageIter& iter, const ObjectPath& p);

   static
   void decode(DBusMessageIter& iter, ObjectPath& p);

   static
   std::ostream& make_type_signature(std::ostream& os);
};


template<>
struct Codec<ObjectPath> : public ObjectPathCodec {};


}   // dbus

}   // simppl


#endif   // SIMPPL_OBJECTPATH_H
