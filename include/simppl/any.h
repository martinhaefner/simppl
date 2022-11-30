#ifndef SIMPPL_ANY_H
#define SIMPPL_ANY_H


#include <sstream>

#include <dbus/dbus.h>

#include "simppl/serialization.h"
#include "simppl/detail/deserialize_and_return.h"


namespace simppl
{

namespace dbus
{

/**
 * A true D-Bus variant that can really hold *anything*.
 */
class Any
{
    friend class Codec<Any>;

    typedef void(*encoder_func_t)(DBusMessageIter&, const void*);
    typedef void(*destructor_func_t)(void*);
    typedef void*(*copy_func_t)(void*);


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


    struct Private
    {
        Private()
         : enc_(nullptr)
         , destr_(nullptr)
         , copy_(nullptr)
         , p_(nullptr)
        {
            //  NOOP
        }


        Private(const Private& rhs)
         : enc_(rhs.enc_)
         , destr_(rhs.destr_)
         , copy_(rhs.copy_)
         , p_(nullptr)
        {
            if (rhs.p_)
            {
                p_ = (*copy_)(rhs.p_);
            }
        }


        template<typename T>
        Private(encoder_func_t e, destructor_func_t d, copy_func_t c, const T& t)
         : enc_(e)
         , destr_(d)
         , copy_(c)
         , p_(new T(t))
        {
            // NOOP
        }


        Private& operator=(const Private& rhs)
        {
            if (&rhs != this)
            {
                if (p_)
                {
                    (*destr_)(p_);
                    p_ = nullptr;
                }

                if (rhs.p_)
                {
                    enc_ = rhs.enc_;
                    destr_ = rhs.destr_;
                    copy_ = rhs.copy_;
                    p_ = (*copy_)(rhs.p_);
                }
            }

            return *this;
        }

        ~Private()
        {
            if (p_)
            {
                (*destr_)(p_);
                p_ = nullptr;
            }
        }

        encoder_func_t enc_;
        destructor_func_t destr_;
        copy_func_t copy_;

        void* p_;
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
            encoder<T>(iter_, &t);
        }

        void operator()(const Private& p) const
        {
            if (p.p_)
                (*p.enc_)(iter_, p.p_);
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

        bool operator()(const Private& p) const
        {
            assert("Not yet implemented");
            return false;
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

        T operator()(const Private& p) const
        {
            assert("Not yet implemented");
            return T();
        }
    };


    template<typename T>
    static
    void encoder(DBusMessageIter& iter, const void* data)
    {
        std::ostringstream buf;
        Codec<T>::make_type_signature(buf);

        DBusMessageIter iter2;
        dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, buf.str().c_str(), &iter2);

        Codec<T>::encode(iter2, *(const T*)data);

        dbus_message_iter_close_container(&iter, &iter2);
    }


    template<typename T>
    static
    void destructor(void* data)
    {
        delete (T*)data;
    }


    template<typename T>
    static
    void* copy(void* data)
    {
        return new T(*(T*)data);
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
     : value_(Private(&encoder<T>, &destructor<T>, &copy<T>, t))
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
        Private p(&encoder<T>, &destructor<T>, &copy<T>, t);
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

    std::variant<Iterator, int, double, std::string, Private> value_;
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

