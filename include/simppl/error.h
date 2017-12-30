#ifndef SIMPPL_ERROR_H
#define SIMPPL_ERROR_H


#include <exception>
#include <cstdint>

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


}   // namespace simppl

}   // namespace dbus


#endif   // SIMPPL_ERROR_H
