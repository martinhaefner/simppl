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
    bool operator<(const ObjectPath& rhs) const
    {
        return path < rhs.path;
    }

    std::string path;
};


}   // dbus

}   // simppl


#endif   // SIMPPL_OBJECTPATH_H
