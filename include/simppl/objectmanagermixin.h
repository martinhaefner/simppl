#ifndef __SIMPPL_DBUS_OBJECTMANAGERMIXIN_H__
#define __SIMPPL_DBUS_OBJECTMANAGERMIXIN_H__

#include <set>

#include "simppl/skeletonbase.h"


namespace simppl
{

namespace dbus
{

class SkeletonBase;

/**
 * If native objectmanager support is enabled, this class
 * extends the API of the Skeletons in case the
 * org.freedesktop.DBus.ObjectManager API is included.
 */
struct ObjectManagerMixin : public SkeletonBase
{
public:

    ObjectManagerMixin(size_t size)
     : SkeletonBase(size)
    {
        // NOOP
    }

    void add_managed_object(SkeletonBase* obj);
    void remove_managed_object(SkeletonBase* obj);

private:

    void serialize_object(DBusMessageIter& iter, const simppl::dbus::SkeletonBase& obj);

    DBusHandlerResult handle_objectmanager_request(DBusMessage* msg);

    std::set<SkeletonBase*> objects_;
};


template<size_t N>
struct SizedObjectManagerMixin : public ObjectManagerMixin
{
    SizedObjectManagerMixin()
     : ObjectManagerMixin(N)
    {
        // NOOP
    }
};

}   // dbus

}   // simppl


#endif   // __SIMPPL_DBUS_OBJECTMANAGERMIXIN_H__

