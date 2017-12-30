#include "simppl/error.h"

#include <cassert>
#include <cstring>

#include "simppl/string.h"


namespace simppl
{

namespace dbus
{
   

/*static*/
std::unique_ptr<Error> Error::from_message(DBusMessage& msg)
{
    DBusMessageIter iter;
    dbus_message_iter_init(&msg, &iter);
 
    std::string text;
    decode(iter, text);
    
    return std::unique_ptr<Error>(new Error(dbus_message_get_error_name(&msg), text.c_str(), dbus_message_get_reply_serial(&msg)));
}


Error::Error(const char* name, const char* msg, uint32_t serial)
 : serial_(serial)
{
    assert(name);
    // TODO check name for valid dbus name (<atom>.<atom>)

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


Error::Error(const Error& rhs)
 : serial_(rhs.serial_)
{
    size_t capacity = strlen(rhs.name_and_message_) + strlen(rhs.message_) + 2;

    name_and_message_ = new char[capacity];
    memset(name_and_message_, 0, capacity);

    message_ = name_and_message_ + (rhs.message_ - rhs.name_and_message_);

    char* p_to = name_and_message_;
    char* p_from = rhs.name_and_message_;

    while(*p_from)
        *p_to++ = *p_from++;

    p_to = message_;
    p_from = rhs.message_;

    while(*p_from)
        *p_to++ = *p_from++;
}


Error::~Error()
{
    delete[] name_and_message_;

    name_and_message_ = nullptr;
    message_ = nullptr;
}


message_ptr_t Error::make_reply_for(DBusMessage& req) const
{
    return make_message(dbus_message_new_error(&req, name(), message()));
}


void Error::_throw()
{
    throw Error(*this);
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
