#ifndef SIMPPL_ANY_H
#define SIMPPL_ANY_H


#include <any>
#include <cassert>
#include <cstring>
#include <memory>
#include <sstream>
#include <variant>

#include <dbus/dbus.h>

#include "simppl/serialization.h"
#include "simppl/detail/deserialize_and_return.h"


namespace simppl
{

namespace dbus
{

/**
 * A true D-Bus variant that can really hold *anything*, as a replacement
 * for variants.
 *
 * Note, that the actual internal data representation of an Any is
 * dependent whether it was created from an actual data or if it was
 * received by a D-Bus method call. A received Any holds a reference on the
 * actual D-Bus stream iterator and cannot be serialized any more into
 * a subsequent D-Bus function call. In order to achieve this, the any
 * has to be deserialized into a variable and this variable can be
 * once again sent as an Any over a D-Bus interface. An example can be seen
 * in the any unittests.
 */
class Any
{
    friend class Codec<Any>;

    typedef void(*encoder_func_t)(DBusMessageIter&, const std::any& a);


    struct Iterator
    {
        Iterator()
         : iter_(DBUS_MESSAGE_ITER_INIT_CLOSED)
         , msg_(nullptr)
        {
            // NOOP
        }

        Iterator(DBusMessage* msg, const DBusMessageIter& iter)
         : iter_(iter)
         , msg_(msg)
        {
            dbus_message_ref(msg_);
        }

        ~Iterator()
        {
            if (msg_)
                dbus_message_unref(msg_);
        }

        Iterator(const Iterator& rhs)
         : iter_(rhs.iter_)
         , msg_(rhs.msg_)
        {
            if (msg_)
                dbus_message_ref(msg_);
        }

        Iterator& operator=(const Iterator& rhs)
        {
            if (&rhs != this)
            {
                if (msg_)
                    dbus_message_unref(msg_);

                iter_ = rhs.iter_;
                msg_ = rhs.msg_;

                if (msg_)
                    dbus_message_ref(msg_);
            }

            return *this;
        }

        DBusMessageIter iter_;
        DBusMessage* msg_;
    };


    struct AnyImpl
    {
        AnyImpl()
         : enc_(nullptr)
        {
            //  NOOP
        }


        AnyImpl(const AnyImpl& rhs)
         : enc_(rhs.enc_)
         , value_(rhs.value_)
        {
            // NOOP
        }


        template<typename T>
        AnyImpl(encoder_func_t e, const T& t)
         : enc_(e)
         , value_(t)
        {
            // NOOP
        }


        AnyImpl& operator=(const AnyImpl& rhs)
        {
            if (&rhs != this)
            {
                enc_ = rhs.enc_;
                value_ = rhs.value_;
            }

            return *this;
        }

        encoder_func_t enc_;

        std::any value_;
    };


    struct EncodingVisitor
    {
        EncodingVisitor(DBusMessageIter& iter)
         : iter_(iter)
        {
            // NOOP
        }

        void operator()(const Iterator&) const
        {
            // never called?!
            assert(false);
        }

        template<typename T>
        void operator()(const T& t) const
        {
            encoder<T>(iter_, t);
        }

        void operator()(const AnyImpl& p) const
        {
            if (p.value_.has_value())
                (*p.enc_)(iter_, p.value_);
        }

    private:

        DBusMessageIter& iter_;
    };


    template<typename T>
    struct TypeVisitor
    {
        TypeVisitor()
        {
            // NOOP
        }

        bool operator()(const Iterator& iter) const
        {
            if (iter.msg_)
            {
                std::ostringstream oss;
                Codec<T>::make_type_signature(oss);

                std::unique_ptr<char, void(*)(void*)> sig(dbus_message_iter_get_signature(const_cast<DBusMessageIter*>(&iter.iter_)), &dbus_free);
                return !strcmp(sig.get(), oss.str().c_str());
            }

            return false;
        }

        template<typename U>
        bool operator()(const U&) const
        {
            return std::is_same<T, U>::value;
        }

        bool operator()(const AnyImpl& p) const
        {
            return std::any_cast<T>(&p.value_) != 0;
        }
    };


    template<typename T>
    struct ExtractionVisitor
    {
        T operator()(const Iterator& iter) const
        {
            if (iter.msg_)
            {
                std::ostringstream oss;
                Codec<T>::make_type_signature(oss);

                std::unique_ptr<char, void(*)(void*)> sig(dbus_message_iter_get_signature(const_cast<DBusMessageIter*>(&iter.iter_)), &dbus_free);

                // TODO some better exception message: expected ..., provided ...
                if (strcmp(sig.get(), oss.str().c_str()))
                    throw std::runtime_error("Invalid type");

                // make a copy, so the method may be called multiple times
                DBusMessageIter __iter = iter.iter_;
                return detail::deserialize_and_return_from_iter<T>::eval(const_cast<DBusMessageIter*>(&__iter));
            }

            throw std::runtime_error("No type");
        }

        template<typename U>
        T operator()(const U& u) const
        {
            // would be nicer to use a switching template
            if (std::is_same_v<T, U>)
                return *(T*)&u;

            throw std::runtime_error("Invalid type");
        }

        T operator()(const AnyImpl& p) const
        {
            return std::any_cast<T>(p.value_);
        }
    };


    template<typename T>
    static
    void any_encoder(DBusMessageIter& iter, const std::any& data)
    {
        encoder<T>(iter, *std::any_cast<T>(&data));
    }

    template<typename T>
    static
    void encoder(DBusMessageIter& iter, const T& data)
    {
        std::ostringstream buf;
        Codec<T>::make_type_signature(buf);

        DBusMessageIter iter2;
        dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, buf.str().c_str(), &iter2);

        Codec<T>::encode(iter2, data);

        dbus_message_iter_close_container(&iter, &iter2);
    }

    void set_message_iterator(DBusMessage* msg, const DBusMessageIter& iter)
    {
        value_ = std::move(Iterator(msg, iter));
    }

    void encode(DBusMessageIter& iter) const
    {
        std::visit(EncodingVisitor(iter), value_);
    }


public:

    /**
     * construction/destruction
     */
    Any()
    {
        // NOOP
    }

    Any(int i)
     : value_(i)
    {
        // NOOP
    }

    Any(double d)
     : value_(d)
    {
        // NOOP
    }

    Any(const std::string& str)
     : value_(str)
    {
        // NOOP
    }

    template<typename T>
    Any(const T& t)
     : value_(AnyImpl(&any_encoder<T>, t))
    {
        // NOOP
    }

    ~Any()
    {
        // NOOP
    }


    /**
     * assignment
     */
    Any& operator=(const Any&) = default;

    template<typename T>
    Any& operator=(const T& t)
    {
        AnyImpl p(&any_encoder<T>, t);
        value_ = p;

        return *this;
    }

    Any& operator=(int i)
    {
        value_ = i;
        return *this;
    }

    Any& operator=(double d)
    {
        value_ = d;
        return *this;
    }

    Any& operator=(const std::string& str)
    {
        value_ = str;
        return *this;
    }


    /**
     * Extraction by value, not reference since the normal use case is to
     * extract the data from a DBus message and not from a preset type.
     */
    template<typename T>
    T as() const
    {
        return std::visit(ExtractionVisitor<T>(), value_);
    }


    template<typename T>
    bool is() const
    {
        return std::visit(TypeVisitor<T>(), value_);
    }


private:

    std::variant<Iterator, int, double, std::string, AnyImpl> value_;
};


template<>
struct Codec<Any>
{
    static
    void encode(DBusMessageIter& iter, const Any& v)
    {
        v.encode(iter);
    }


    static
    void decode(DBusMessageIter& orig, Any& v)
    {
        DBusMessageIter iter;
        simppl_dbus_message_iter_recurse(&orig, &iter, DBUS_TYPE_VARIANT);

        // FIXME TODO XXX take the message from the codec calls since this is
        // highly implementation dependent
        v.set_message_iterator((DBusMessage*)iter.dummy1, iter);

        dbus_message_iter_next(&orig);
    }


    static inline
    std::ostream& make_type_signature(std::ostream& os)
    {
        return os << DBUS_TYPE_VARIANT_AS_STRING;
    }
};

}   // dbus

}   // simppl


#endif   // SIMPPL_ANY_H

