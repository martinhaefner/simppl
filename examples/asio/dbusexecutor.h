#ifndef __SIMPPL_DBUSEXECUTOR_H__
#define __SIMPPL_DBUSEXECUTOR_H__


// forward decl
namespace boost { namespace asio {
    class io_context;
} }

namespace simppl { namespace dbus {
    class Dispatcher;
} }


/**
 * ASIO eventloop integration.
 *
 * Instantiate an object ob this type in order to allow D-Bus callouts
 * be handled by ASIO.
 */
class DBusExecutor
{
    DBusExecutor(const DBusExecutor&) = delete;
    DBusExecutor& operator=(const DBusExecutor&) = delete;

public:

    DBusExecutor(boost::asio::io_context& io_context);
    ~DBusExecutor();

    /**
     * @return Dispatcher object to register stubs and skeletons on.
     */
    simppl::dbus::Dispatcher& dispatcher();

private:

    struct Private;
    Private* d;
};


#endif   // __SIMPPL_DBUSEXECUTOR_H__
