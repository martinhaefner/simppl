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

// forward decl
namespace detail {
    template<typename> struct ErrorFactory;
}


struct Error : public std::exception
{
    template<typename> friend struct detail::ErrorFactory;
    template<typename> friend struct TCallState;
    template<typename...> friend struct ClientMethod;

    // no copies
    Error(const Error&) = delete;
    Error& operator=(const Error&) = delete;

    /**
     * @param name must be a valid dbus name, i.e. <atom>.<atom>
     */
    Error(const char* name, const char* msg = nullptr, uint32_t serial = SIMPPL_INVALID_SERIAL);

    // allow move
    Error(Error&& rhs);

    ~Error();

    const char* what() const throw();

    const char* name() const;
    const char* message() const;

    inline
    uint32_t serial() const
    {
        return serial_;
    }


protected:

    message_ptr_t make_reply_for(DBusMessage& req) const;

    void set_members(const char* name, const char* msg, uint32_t serial);

    Error();


    char* name_and_message_;
    char* message_;

    uint32_t serial_;
};


}   // namespace simppl

}   // namespace dbus


#endif   // SIMPPL_ERROR_H
