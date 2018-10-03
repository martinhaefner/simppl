#ifndef SIMPPL_ERROR_H
#define SIMPPL_ERROR_H


#include <exception>
#include <cstdint>
#include <stdexcept>
#include <sstream>

#include "simppl/detail/constants.h"
#include "simppl/types.h"


namespace simppl
{

namespace dbus
{


struct Error : public std::exception
{
    friend struct CallState;
    friend struct SkeletonBase;
   
   
    Error& operator=(const Error&) = delete;

    Error(const char* name, const char* msg = nullptr, uint32_t serial = SIMPPL_INVALID_SERIAL);

    Error(const Error& rhs);

    ~Error();

    message_ptr_t make_reply_for(DBusMessage& req) const;
    void _throw();

    const char* what() const throw();

    const char* name() const;
    const char* message() const;

    inline
    uint32_t serial() const
    {
        return serial_;
    }

private:

    static std::unique_ptr<Error> from_message(DBusMessage& msg);

    char* name_and_message_;
    char* message_;

    uint32_t serial_;
};

class RuntimeError : public std::runtime_error
{
public:
    RuntimeError(const char* action, ::DBusError&& error)
      : std::runtime_error(format_what(action, error))
    {
        dbus_error_init(&error_);
        dbus_move_error(&error, &error_);
    }

    virtual ~RuntimeError()
    {
        dbus_error_free(&error_);
    }

    const char* name() const noexcept
    {
        return error_.name;
    }

    const char* message() const noexcept
    {
        return error_.message;
    }

private:
    static std::string format_what(const char* action, const ::DBusError& error)
    {
        std::ostringstream ss;
        ss << action << ": " << error.name << error.message;
        return ss.str();
    }

private:
    ::DBusError error_;
};

}   // namespace simppl

}   // namespace dbus


#endif   // SIMPPL_ERROR_H
