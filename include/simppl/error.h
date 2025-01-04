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

// forward decl
namespace detail {
    template<typename> struct ErrorFactory;
}


struct Error : public std::exception
{
    template<typename> friend struct detail::ErrorFactory;
    friend struct CallState;
    template<typename...> friend struct ClientMethod;
    friend struct StubBase;

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

    message_ptr_t make_reply_for(DBusMessage& req, const char* classname = nullptr) const;

    void set_members(const char* name, const char* msg, uint32_t serial);

    Error();


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
