#include "simppl/objectmanagermixin.h"

#include <cassert>

#include "simppl/serverside.h"
#include "simppl/dispatcher.h"
#include "simppl/objectpath.h"


void simppl::dbus::ObjectManagerMixin::serialize_object(DBusMessageIter& _iter, const simppl::dbus::SkeletonBase& obj)
{
    DBusMessageIter iter[4];

    encode(_iter, simppl::dbus::ObjectPath(obj.objectpath()));

    // array of interfaces
    dbus_message_iter_open_container(&_iter, DBUS_TYPE_ARRAY, "{sa{sv}}", &iter[0]);

    for(size_t i=0; i<obj.ifaces_.size(); ++i)
    {
        dbus_message_iter_open_container(&iter[0], DBUS_TYPE_DICT_ENTRY, nullptr, &iter[1]);
        encode(iter[1], obj.ifaces_[i]);

        // array of properties
        dbus_message_iter_open_container(&iter[1], DBUS_TYPE_ARRAY, "{sv}", &iter[2]);

        for (auto pp = obj.property_heads_[i]; pp; pp = pp->next_)
        {
            dbus_message_iter_open_container(&iter[2], DBUS_TYPE_DICT_ENTRY, nullptr, &iter[3]);

            encode(iter[3], pp->name_);
            pp->eval(&iter[3]);

            dbus_message_iter_close_container(&iter[2], &iter[3]);
        }

        dbus_message_iter_close_container(&iter[1], &iter[2]);
        dbus_message_iter_close_container(&iter[0], &iter[1]);
    }

    dbus_message_iter_close_container(&_iter, &iter[0]);
}


void simppl::dbus::ObjectManagerMixin::add_managed_object(simppl::dbus::SkeletonBase* obj)
{
    assert(obj);

    objects_.insert(obj);

    message_ptr_t msg = make_message(dbus_message_new_signal(objectpath(), "org.freedesktop.DBus.ObjectManager", "InterfacesAdded"));

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg.get(), &iter);

    serialize_object(iter, *obj);

    dbus_connection_send(disp_->conn_, msg.get(), nullptr);
}


void simppl::dbus::ObjectManagerMixin::remove_managed_object(simppl::dbus::SkeletonBase* obj)
{
    assert(obj);

    objects_.erase(obj);

    message_ptr_t msg = make_message(dbus_message_new_signal(objectpath(), "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved"));

    DBusMessageIter iter[2];
    dbus_message_iter_init_append(msg.get(), &iter[0]);

    encode(iter[0], simppl::dbus::ObjectPath(obj->objectpath()));

    dbus_message_iter_open_container(&iter[0], DBUS_TYPE_ARRAY, "s", &iter[1]);

    for(size_t i=0; i<obj->ifaces_.size(); ++i)
    {
        encode(iter[1], obj->ifaces_[i]);
    }

    dbus_message_iter_close_container(&iter[0], &iter[1]);

    dbus_connection_send(disp_->conn_, msg.get(), nullptr);
}


DBusHandlerResult simppl::dbus::ObjectManagerMixin::handle_objectmanager_request(DBusMessage* msg)
{
    message_ptr_t response = make_message(dbus_message_new_method_return(msg));

    DBusMessageIter iter[3];

    // array of object paths
    dbus_message_iter_init_append(response.get(), &iter[0]);
    dbus_message_iter_open_container(&iter[0], DBUS_TYPE_ARRAY, "{oa{sa{sv}}}", &iter[1]);

    for (auto oiter = objects_.begin(); oiter != objects_.end(); ++oiter)
    {
        dbus_message_iter_open_container(&iter[1], DBUS_TYPE_DICT_ENTRY, nullptr, &iter[2]);

        serialize_object(iter[2], *(*oiter));

        dbus_message_iter_close_container(&iter[1], &iter[2]);
    }

    dbus_message_iter_close_container(&iter[0], &iter[1]);
    dbus_connection_send(disp_->conn_, response.get(), nullptr);

    return DBUS_HANDLER_RESULT_HANDLED;
}
