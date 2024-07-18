#include "dbusexecutor.h"

#include <map>

#include <dbus/dbus.h>

#include "boost/asio.hpp"
#include "boost/system/error_code.hpp"

#include <simppl/dispatcher.h>


namespace asio = boost::asio;


namespace {

struct Timeout
{
    Timeout(asio::io_context& io_service, DBusTimeout* timeout)
     : timer_(io_service)
     , timeout_(timeout)
    {
        assert(timeout);
    }


    ~Timeout()
    {
        cancel();
    }


    void start()
    {
        timer_.expires_from_now(std::chrono::milliseconds(dbus_timeout_get_interval(timeout_)));

        timer_.async_wait([this](const boost::system::error_code& ec){
            if (ec == asio::error::operation_aborted)
                return;

            if (ec)
                return;

            dbus_timeout_handle(timeout_);
        });
    }


    inline
    void cancel()
    {
        timer_.cancel();
    }


    asio::steady_timer timer_;
    DBusTimeout* timeout_;
};


struct Watch
{
    Watch(const std::shared_ptr<asio::posix::stream_descriptor>& device, DBusWatch* watch)
     : stream_descriptor_(device)
     , watch_(watch)
    {
        assert(watch);
    }


    ~Watch() noexcept
    {
        stream_descriptor_->cancel();

        // the last watch holding the fd -> drop it, DBusConnection will close it
        if (stream_descriptor_.use_count() == 1)
            stream_descriptor_->release();
    }


    void start()
    {
        restart();
    }


    void restart()
    {
        int watch_flags = dbus_watch_get_flags(watch_);

        if (watch_flags & DBUS_WATCH_READABLE)
        {
            stream_descriptor_->async_read_some(
                asio::null_buffers(), [this](const boost::system::error_code& error, std::size_t){
                    if (error == asio::error::operation_aborted)
                        return;

                    this->handle_event(DBUS_WATCH_READABLE, error);
                });
        }

        if (watch_flags & DBUS_WATCH_WRITABLE)
        {
            stream_descriptor_->async_write_some(
                asio::null_buffers(), [this](const boost::system::error_code& error, std::size_t){
                    if (error == asio::error::operation_aborted)
                        return;

                    this->handle_event(DBUS_WATCH_WRITABLE, error);
                });
        }
    }


    void cancel()
    {
        stream_descriptor_->cancel();
    }


    void handle_event(int event, const boost::system::error_code& error)
    {
        if (error)
        {
            dbus_watch_handle(watch_, DBUS_WATCH_ERROR);
        }
        else
        {
            if (!dbus_watch_handle(watch_, event))
                throw std::runtime_error("Insufficient memory while handling watch event");

            restart();
        }
    }


    std::shared_ptr<asio::posix::stream_descriptor> stream_descriptor_;
    DBusWatch* watch_;
};


template<typename T>
void generic_deleter(void* p)
{
    delete (T*)p;
}


}   // anon namespace


// ---------------------------------------------------------------------


struct DBusExecutor::Private
{
    static
    dbus_bool_t on_dbus_add_watch(DBusWatch* watch, void* data)
    {
        auto that = static_cast<Private*>(data);

        // see if we already have a watch on that fd...
        int fd = dbus_watch_get_unix_fd(watch);

        std::shared_ptr<asio::posix::stream_descriptor> device_ptr;

        auto iter = that->devices_.find(fd);
        if (iter != that->devices_.end())
        {
            device_ptr = iter->second;
        }
        else
        {
            // no, create new asio descriptor device object
            device_ptr = std::make_shared<asio::posix::stream_descriptor>(that->context_, fd);
            that->devices_[fd] = device_ptr;
        }

        auto w = new Watch(device_ptr, watch);

        dbus_watch_set_data(watch, w, &generic_deleter<Watch>);

        if (dbus_watch_get_enabled(watch) == TRUE)
            w->start();

        return TRUE;
    }


    static
    void on_dbus_remove_watch(DBusWatch* watch, void* data)
    {
        auto that = static_cast<Private*>(data);

        auto w = static_cast<Watch*>(dbus_watch_get_data(watch));

        if (!w)
            return;

        int fd = dbus_watch_get_unix_fd(watch);

        w->cancel();

        auto iter = that->devices_.find(fd);
        if (iter != that->devices_.end())
        {
            // decrease ref count and drop object if adequate but do never close the descriptor.
            // Watch will release the descriptor when ref count falls back to 1.
            if (iter->second.use_count() == 2)
                that->devices_.erase(iter);
        }
    }


    static
    void on_dbus_watch_toggled(DBusWatch* watch, void*)
    {
        auto w = static_cast<Watch*>(dbus_watch_get_data(watch));

        if (!w)
            return;

        if (dbus_watch_get_enabled(watch) == TRUE)
        {
            w->restart();
        }
        else
            w->cancel();
    }


    static
    dbus_bool_t on_dbus_add_timeout(DBusTimeout* timeout, void* data)
    {
        auto that = static_cast<Private*>(data);

        auto tm = new Timeout(that->context_, timeout);

        dbus_timeout_set_data(timeout, tm, &generic_deleter<Timeout>);

        tm->start();

        return TRUE;
    }


    static
    void on_dbus_remove_timeout(DBusTimeout* timeout, void*)
    {
        static_cast<Timeout*>(dbus_timeout_get_data(timeout))->cancel();
    }


    static
    void on_dbus_timeout_toggled(DBusTimeout* timeout, void*)
    {
        auto tm = static_cast<Timeout*>(dbus_timeout_get_data(timeout));

        if (!tm)
            return;

        if (dbus_timeout_get_enabled(timeout) == TRUE)
        {
            tm->start();
        }
        else
            tm->cancel();
    }


    static
    void on_dbus_wakeup_event_loop(void* data)
    {
        auto that = static_cast<Private*>(data);
        auto& dispatcher = that->dispatcher_;

        asio::post(that->context_, [&dispatcher](){
            dispatcher.dispatch();
        });
    }


    explicit
    Private(asio::io_context& context)
     : context_(context)
     , dispatcher_("bus:session")
    {
        if (!dbus_connection_set_watch_functions(
                &dispatcher_.connection(),
                on_dbus_add_watch,
                on_dbus_remove_watch,
                on_dbus_watch_toggled,
                this,
                nullptr))
            throw std::runtime_error("Problem installing watch functions.");

        if (!dbus_connection_set_timeout_functions(
                &dispatcher_.connection(),
                on_dbus_add_timeout,
                on_dbus_remove_timeout,
                on_dbus_timeout_toggled,
                this,
                nullptr))
            throw std::runtime_error("Problem installing timeout functions.");

        dbus_connection_set_wakeup_main_function(
            &dispatcher_.connection(),
            on_dbus_wakeup_event_loop,
            this,
            nullptr);
    }


    ~Private() noexcept
    {
        // NOOP
    }


    asio::io_context& context_;

    // keep this ordering!
    std::map<int, std::shared_ptr<asio::posix::stream_descriptor>> devices_;
    simppl::dbus::Dispatcher dispatcher_;
};


// ---------------------------------------------------------------------


DBusExecutor::DBusExecutor(asio::io_context& io_context)
 : d(new Private(io_context))
{
    // NOOP
}


DBusExecutor::~DBusExecutor()
{
    delete d;
}


simppl::dbus::Dispatcher& DBusExecutor::dispatcher()
{
    return d->dispatcher_;
}
