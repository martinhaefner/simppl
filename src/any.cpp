#include "simppl/any.h"

#include <unistd.h>


namespace simppl
{

namespace dbus
{

namespace detail
{

void copy_value(DBusMessageIter& from, DBusMessageIter& to)
{
    const int type = dbus_message_iter_get_arg_type(&from);

    if (dbus_type_is_basic(type))
    {
        DBusBasicValue value;
        dbus_message_iter_get_basic(&from, &value);
        dbus_message_iter_append_basic(&to, type, &value);

        // get_basic hands out a duplicated descriptor, append_basic duplicates again
        if (type == DBUS_TYPE_UNIX_FD)
            ::close(value.fd);
    }
    else
    {
        DBusMessageIter sub_from;
        dbus_message_iter_recurse(&from, &sub_from);

        std::unique_ptr<char, void(*)(void*)> sig(nullptr, &dbus_free);
        const char* contained = nullptr;

        if (type == DBUS_TYPE_ARRAY)
        {
            sig.reset(dbus_message_iter_get_signature(&from));
            contained = sig.get() + 1;   // skip the 'a', works for empty arrays too
        }
        else if (type == DBUS_TYPE_VARIANT)
        {
            sig.reset(dbus_message_iter_get_signature(&sub_from));
            contained = sig.get();
        }

        DBusMessageIter sub_to;
        dbus_message_iter_open_container(&to, type, contained, &sub_to);

        const int element_type = type == DBUS_TYPE_ARRAY ? dbus_message_iter_get_element_type(&from) : DBUS_TYPE_INVALID;

        if (dbus_type_is_fixed(element_type) && element_type != DBUS_TYPE_UNIX_FD)
        {
            // array of fixed size elements in one go
            const void* data = nullptr;
            int n = 0;

            dbus_message_iter_get_fixed_array(&sub_from, &data, &n);
            dbus_message_iter_append_fixed_array(&sub_to, element_type, &data, n);
        }
        else
        {
            while (dbus_message_iter_get_arg_type(&sub_from) != DBUS_TYPE_INVALID)
                copy_value(sub_from, sub_to);
        }

        dbus_message_iter_close_container(&to, &sub_to);
    }

    dbus_message_iter_next(&from);
}

}   // namespace detail


void Any::encode(DBusMessageIter& iter) const
{
    DBusMessageIter variant;

    if (auto l = std::get_if<Local>(&value_))
    {
        dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, l->signature_, &variant);
        (*l->encode_)(variant, l->value_);
    }
    else if (auto r = std::get_if<Received>(&value_))
    {
        DBusMessageIter from = r->iter_;

        dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, received_signature(*r).get(), &variant);
        detail::copy_value(from, variant);
    }
    else
        throw std::logic_error("Cannot send an empty Any");

    dbus_message_iter_close_container(&iter, &variant);
}


/*static*/
void Codec<Any>::decode(DBusMessageIter& iter, Any& v)
{
    DBusMessageIter variant;
    simppl_dbus_message_iter_recurse(&iter, &variant, DBUS_TYPE_VARIANT);

    if (DBusMessage* msg = DecodingScope::current())
    {
        v.value_ = Any::Received(msg, variant);
    }
    else
    {
        // unknown source message, keep a private copy of the value
        std::unique_ptr<DBusMessage, void(*)(DBusMessage*)> copy(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL), &dbus_message_unref);

        DBusMessageIter to;
        dbus_message_iter_init_append(copy.get(), &to);
        detail::copy_value(variant, to);

        dbus_message_iter_init(copy.get(), &to);
        v.value_ = Any::Received(copy.get(), to);
    }

    dbus_message_iter_next(&iter);
}


}   // namespace dbus

}   // namespace simppl
