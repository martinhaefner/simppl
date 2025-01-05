#ifndef SIMPPL_DETAIL_ERROR_H
#define SIMPPL_DETAIL_ERROR_H

#include <iostream>  // FIXME remove this

#include "simppl/types.h"
#include "simppl/string.h"

#include "util.h"

#include <cxxabi.h>


namespace simppl
{

namespace dbus
{

namespace detail
{


// FIXME get rid of redefinion
struct FreeDeleter {
    template<typename T>
    void operator()(T* o) {
        ::free(o);
    }
};


template<typename ExceptionT>
struct ErrorFactory
{
    /// server side
    static
    message_ptr_t reply(DBusMessage& msg, const Error& e)
    {
        std::unique_ptr<char, FreeDeleter> name;

        if (!e.name())
            name.reset(make_error_name(abi::__cxa_demangle(typeid(ExceptionT).name(), nullptr, 0, nullptr)));

        message_ptr_t rmsg = e.make_reply_for(msg, name.get());

        // encode arguments
        DBusMessageIter iter;
        dbus_message_iter_init_append(rmsg.get(), &iter);

        const ExceptionT* exp = dynamic_cast<const ExceptionT*>(&e);
        if (exp)
            encode(iter, *exp);

        return rmsg;
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
        if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INVALID)
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
        return rmsg;
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
