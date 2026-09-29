#ifndef SIMPPL_ANY_H
#define SIMPPL_ANY_H


#include <any>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <variant>

#include <dbus/dbus.h>

#include "simppl/serialization.h"
#include "simppl/string.h"


namespace simppl
{

namespace dbus
{

namespace detail
{

/**
 * Copy the value 'from' points to into 'to' and advance 'from'. Works for
 * any D-Bus type, the value is walked along its signature.
 */
void copy_value(DBusMessageIter& from, DBusMessageIter& to);

}   // namespace detail


/**
 * A true D-Bus variant that can really hold *anything*, as a replacement
 * for variants.
 *
 * An Any behaves the same, no matter if it was created from data or
 * received over D-Bus: is<T>() and as<T>() compare D-Bus signatures, so an
 * Any holding an enum is also an int32_t. A received Any may be sent again.
 *
 * Internally, a locally created Any holds the typed value, a received Any
 * holds a reference on the received message, so no data is copied.
 */
class Any
{
    friend struct Codec<Any>;

    typedef void(*encoder_type)(Encoder&, const std::any&);

    struct Local
    {
        std::any value_;
        const char* signature_;
        encoder_type encode_;
    };

    struct Received
    {
        Received(DBusMessage* msg, const DBusMessageIter& iter)
         : msg_(dbus_message_ref(msg))
         , iter_(iter)
        {
            // NOOP
        }

        Received(const Received& rhs)
         : msg_(dbus_message_ref(rhs.msg_))
         , iter_(rhs.iter_)
        {
            // NOOP
        }

        Received& operator=(const Received& rhs)
        {
            Received tmp(rhs);
            std::swap(msg_, tmp.msg_);
            iter_ = rhs.iter_;

            return *this;
        }

        ~Received()
        {
            dbus_message_unref(msg_);
        }

        DBusMessage* msg_;
        DBusMessageIter iter_;
    };

    typedef std::unique_ptr<char, void(*)(void*)> signature_ptr;


    template<typename T>
    static
    void encoder(Encoder& e, const std::any& data)
    {
        detail::encode_one<T>(e, *std::any_cast<T>(&data));
    }


    static
    signature_ptr received_signature(const Received& r)
    {
        return signature_ptr(dbus_message_iter_get_signature(const_cast<DBusMessageIter*>(&r.iter_)), &dbus_free);
    }


    template<typename T>
    static
    T decode_from(DBusMessage* msg, const DBusMessageIter& iter)
    {
        // nested Anys refer to the same message
        DBusMessageIter _iter = iter;
        Decoder d(_iter, msg);

        T t;
        detail::decode_one<T>(d, t);

        return t;
    }


    void encode(Encoder& e) const;


public:

    /**
     * An empty Any, is<T>() is false for all T and it cannot be sent.
     */
    Any() = default;

    template<typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Any>::value>>
    Any(const T& t)
     : value_(Local{ t, signature_of<T>(), &encoder<T> })
    {
        // NOOP
    }

    Any(const char* str)
     : Any(std::string(str))
    {
        // NOOP
    }


    template<typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Any>::value>>
    Any& operator=(const T& t)
    {
        return *this = Any(t);
    }


    /**
     * @return true if the contained D-Bus type matches the D-Bus type of T.
     */
    template<typename T>
    bool is() const
    {
        if (auto l = std::get_if<Local>(&value_))
            return !strcmp(l->signature_, signature_of<T>());

        if (auto r = std::get_if<Received>(&value_))
            return !strcmp(received_signature(*r).get(), signature_of<T>());

        return false;
    }


    /**
     * Extraction by value, not reference since the normal use case is to
     * extract the data from a DBus message and not from a preset type.
     *
     * @throw std::runtime_error if the D-Bus type does not match.
     */
    template<typename T>
    T as() const
    {
        if (auto l = std::get_if<Local>(&value_))
        {
            // fast path, exactly the type the Any was created with
            if (auto p = std::any_cast<T>(&l->value_))
                return *p;

            if (strcmp(l->signature_, signature_of<T>()))
                throw std::runtime_error("Invalid type");

            // same D-Bus type, other C++ type: convert via the wire format
            std::unique_ptr<DBusMessage, void(*)(DBusMessage*)> msg(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL), &dbus_message_unref);

            {
                Encoder e(msg.get());
                (*l->encode_)(e, l->value_);
            }

            DBusMessageIter iter;
            dbus_message_iter_init(msg.get(), &iter);
            return decode_from<T>(msg.get(), iter);
        }

        if (auto r = std::get_if<Received>(&value_))
        {
            // TODO some better exception message: expected ..., provided ...
            if (strcmp(received_signature(*r).get(), signature_of<T>()))
                throw std::runtime_error("Invalid type");

            return decode_from<T>(r->msg_, r->iter_);
        }

        throw std::runtime_error("No type");
    }


private:

    std::variant<std::monostate, Local, Received> value_;
};


template<>
struct Codec<Any> : composite_signature<signature_chars<DBUS_TYPE_VARIANT>>
{
    static
    void encode(Encoder& e, const Any& v)
    {
        v.encode(e);
    }


    static
    void decode(Decoder& d, Any& v);
};

}   // dbus

}   // simppl


#endif   // SIMPPL_ANY_H
