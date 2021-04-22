#include "simppl/error.h"

#include <cassert>
#include <cstring>

#include "simppl/string.h"


namespace simppl
{

namespace dbus
{


Error::Error(const char* name, const char* msg, uint32_t serial)
 : name_and_message_(nullptr)
 , message_(nullptr)
 , serial_(SIMPPL_INVALID_SERIAL)
{
    assert(name);

    // TODO check name for valid dbus name (<atom>.<atom>)
    set_members(name, msg, serial);
}


Error::Error()
 : name_and_message_(nullptr)
 , message_(nullptr)
 , serial_(SIMPPL_INVALID_SERIAL)
{
    // NOOP
}


Error::Error(Error&& rhs)
{
    name_and_message_ = rhs.name_and_message_;
    message_ = rhs.message_;
    serial_ = rhs.serial_;

    rhs.name_and_message_ = nullptr;
    rhs.message_ = nullptr;
    rhs.serial_ = 0;  // FIXME INVALID
}


Error::~Error()
{
    delete[] name_and_message_;

    name_and_message_ = nullptr;
    message_ = nullptr;
}


void Error::set_members(const char* name, const char* msg, uint32_t serial)
{
    if (name_and_message_)
        delete[] name_and_message_;

    serial_ = serial;

    size_t capacity = strlen(name) + 1;

    if (msg && strlen(msg))
        capacity += strlen(msg)+1;

    name_and_message_ = new char[capacity];

    strcpy(name_and_message_, name);

    if (msg && strlen(msg))
    {
        message_ = name_and_message_ + strlen(name) + 1;
        strcpy(message_, msg);
    }
    else
        message_ = name_and_message_ + strlen(name_and_message_);
}


message_ptr_t Error::make_reply_for(DBusMessage& req) const
{
    return make_message(dbus_message_new_error(&req, name(), message()));
}


const char* Error::what() const throw()
{
    return name_and_message_;
}


const char* Error::name() const
{
    return name_and_message_;
}


const char* Error::message() const
{
    return message_;
}


}   // namespace dbus

}   // namespace simppl
