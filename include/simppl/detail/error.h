#ifndef SIMPPL_DETAIL_ERROR_H
#define SIMPPL_DETAIL_ERROR_H


#include "simppl/types.h"
#include "simppl/string.h"


namespace simppl
{

namespace dbus
{

namespace detail
{


template<typename ExceptionT>
struct ErrorFactory
{
    /// server side
    static
    message_ptr_t reply(DBusMessage& msg, const Error& e)
    {
        message_ptr_t rmsg = e.make_reply_for(msg);

        // encode arguments
        DBusMessageIter iter;
        dbus_message_iter_init_append(rmsg.get(), &iter);

        const ExceptionT* exp = dynamic_cast<const ExceptionT*>(&e);
        if (exp)
            encode(iter, *exp);

        return std::move(rmsg);
    }


    /// client side
    static
    void init(ExceptionT& err, DBusMessage& msg)
    {
        DBusMessageIter iter;
        dbus_message_iter_init(&msg, &iter);

        std::string text;
        decode(iter, text);

        // set default members
        err.set_members(dbus_message_get_error_name(&msg), text.c_str(), dbus_message_get_reply_serial(&msg));

        // any other unexpected dbus error, e.g. exception during method body
        if (dbus_message_iter_has_next(&iter))
        {
            // and now the rest
            decode(iter, err);
        }
    }
};

template<>
struct ErrorFactory<Error>
{
    static
    message_ptr_t reply(DBusMessage& msg, const Error& e)
    {
        message_ptr_t rmsg = e.make_reply_for(msg);
        return std::move(rmsg);
    }


    static
    void init(Error& err, DBusMessage& msg)
    {
        DBusMessageIter iter;
        dbus_message_iter_init(&msg, &iter);

        std::string text;
        decode(iter, text);

        err.set_members(dbus_message_get_error_name(&msg), text.c_str(), dbus_message_get_reply_serial(&msg));
    }
};


}   // detail

}   // dbus

}   // simppl


#endif   // SIMPPL_DETAIL_ERROR_H
