#include "simppl/dispatcher.h"

#include <sys/timerfd.h>
#include <sys/poll.h>

#include <unistd.h>

#include <iostream>
#include <map>
#include <set>
#include <atomic>

#include "simppl/detail/util.h"
#include "simppl/timeout.h"
#include "simppl/skeletonbase.h"
#include "simppl/string.h"
#include "simppl/vector.h"

#define SIMPPL_DISPATCHER_CPP
#include "simppl/serverside.h"
#include "simppl/clientside.h"
#undef SIMPPL_DISPATCHER_CPP

#define SIMPPL_USE_POLL

using namespace std::placeholders;
using namespace std::literals::chrono_literals;


namespace
{

std::string generate_matchstring(simppl::dbus::StubBase& stub, const char* signame)
{
   std::ostringstream match_string;

   match_string
      << "type='signal'"
      << ", sender='" << stub.busname() << "'"
      << ", interface='" << stub.iface() << "'"
      << ", member='" << signame << "'";

   return match_string.str();
}


std::string generate_property_matchstring(simppl::dbus::StubBase& stub)
{
   std::ostringstream match_string;

   match_string
      << "type='signal'"
      << ",sender='" << stub.busname() << "'"
      << ",interface='org.freedesktop.DBus.Properties'"
      << ",member='PropertiesChanged'"
      << ",path='" << stub.objectpath() << "'";

   return match_string.str();
}


inline
std::chrono::steady_clock::time_point
get_lookup_duetime()
{
   return std::chrono::steady_clock::now() + 5s;
}


DBusHandlerResult signal_filter(DBusConnection* /*connection*/, DBusMessage* msg, void *user_data)
{
    simppl::dbus::Dispatcher* disp = (simppl::dbus::Dispatcher*)user_data;
    return disp->try_handle_signal(msg);
}


short make_poll_events(int flags)
{
    short rc = 0;

    if (flags & DBUS_WATCH_READABLE)
        rc |= POLLIN;
    if (flags & DBUS_WATCH_WRITABLE)
        rc |= POLLOUT;

    // do not expect to have read and write simultaneously from DBus API
    assert(flags | ((DBUS_WATCH_READABLE&DBUS_WATCH_WRITABLE) != (DBUS_WATCH_READABLE&DBUS_WATCH_WRITABLE)));

    return rc;
}


int make_dbus_flags(short events)
{
    int rc = 0;

    if (events & POLLIN)
        rc |= DBUS_WATCH_READABLE;
    if (events & POLLOUT)
        rc |= DBUS_WATCH_WRITABLE;
    if (events & POLLHUP)
        rc |= DBUS_WATCH_HANGUP;
    if (events & POLLERR)
        rc |= DBUS_WATCH_ERROR;

    return rc;
}


}   // namespace


// ---------------------------------------------------------------------------------------


namespace simppl
{

namespace dbus
{


// --- need this in order to resolve cyclic dependencies ---------------


void dispatcher_add_stub(Dispatcher& disp, StubBase& stub)
{
    disp.add_client(stub);
}


void dispatcher_add_skeleton(Dispatcher& disp, SkeletonBase& stub)
{
    disp.add_server(stub);
}


// ---------------------------------------------------------------------


struct Dispatcher::Private
{
    static
    dbus_bool_t add_watch(DBusWatch *watch, void *data)
    {
        return ((simppl::dbus::Dispatcher::Private*)data)->add_watch(watch);
    }

    static
    void remove_watch(DBusWatch *watch, void *data)
    {
        ((simppl::dbus::Dispatcher::Private*)data)->remove_watch(watch);
    }

    static
    void toggle_watch(DBusWatch *watch, void *data)
    {
        ((simppl::dbus::Dispatcher::Private*)data)->toggle_watch(watch);
    }

    static
    dbus_bool_t add_timeout(DBusTimeout *timeout, void *data)
    {
        return ((simppl::dbus::Dispatcher::Private*)data)->add_timeout(timeout);
    }

    static
    void remove_timeout(DBusTimeout *timeout, void *data)
    {
        ((simppl::dbus::Dispatcher::Private*)data)->remove_timeout(timeout);
    }

    static
    void toggle_timeout(DBusTimeout *timeout, void *data)
    {
        ((simppl::dbus::Dispatcher::Private*)data)->toggle_timeout(timeout);
    }


    Private()
     : running_(false)
    {
      // NOOP
    }


    dbus_bool_t add_watch(DBusWatch* w)
    {
       //std::cout << "add_watch" << std::endl;
        pollfd fd = { 0 };

        fd.fd = dbus_watch_get_unix_fd(w);

        if (dbus_watch_get_enabled(w))
        {
            fd.events = make_poll_events(dbus_watch_get_flags(w));
            fds_.push_back(fd);
        }
        //else
          // std::cout << "Not enabled" << std::endl;

        watch_handlers_.insert(std::make_pair(fd.fd, w));

        return TRUE;
    }


    void remove_watch(DBusWatch* w)
    {
       //std::cout << "remove_watch" << std::endl;
       auto result = watch_handlers_.equal_range(dbus_watch_get_unix_fd(w));

       for(auto iter = result.first; iter != result.second; ++iter)
       {
          if (iter->second == w)
          {
             auto pfditer = std::find_if(fds_.begin(), fds_.end(), [w](auto& pfd){
                return dbus_watch_get_unix_fd(w) == pfd.fd
                    && pfd.revents & make_poll_events(dbus_watch_get_flags(w));
             });

             if (pfditer != fds_.end())
             {
//                std::cout << "ok found" << std::endl;
                 fds_.erase(pfditer);
             }
             watch_handlers_.erase(iter);
             break;
          }
       }
    }


    void toggle_watch(DBusWatch* w)
    {
       assert(false);   // not implemented
        /*   auto iter = std::find_if(fds_.begin(), fds_.end(), [w](auto& pfd){ return dbus_watch_get_unix_fd(w) == pfd.fd; });
        if (iter != fds_.end())
        {
            if (dbus_watch_get_enabled(w))
            {
                iter->events = make_poll_events(dbus_watch_get_flags(w));
            }
            else
                iter->events = 0;
        }*/
    }


    dbus_bool_t add_timeout(DBusTimeout* t)
    {
        pollfd fd;
        fd.fd = timerfd_create(CLOCK_MONOTONIC, 0);
        fd.events = POLLIN;

        //std::cout << "add_timeout fd=" << fd.fd << std::endl;

        if (dbus_timeout_get_enabled(t))
        {
            int interval_ms = dbus_timeout_get_interval(t);

            struct itimerspec spec = {
                { interval_ms / 1000, (interval_ms % 1000) * 1000000L },
                { interval_ms / 1000, (interval_ms % 1000) * 1000000L }
            };

            timerfd_settime(fd.fd, 0, &spec, 0);
        }

        dbus_timeout_set_data(t, reinterpret_cast<void*>(fd.fd), 0);

        fds_.push_back(fd);
        tm_handlers_[fd.fd] = t;

        return TRUE;
    }


    void remove_timeout(DBusTimeout* t)
    {
        //std::cout << "remove_timeout fd=" << reinterpret_cast<int>(dbus_timeout_get_data(t)) << std::endl;
        auto iter = std::find_if(fds_.begin(), fds_.end(), [t](auto& pfd){ return reinterpret_cast<int64_t>(dbus_timeout_get_data(t)) == pfd.fd; });
        if (iter != fds_.end())
        {
            tm_handlers_.erase(iter->fd);
            ::close(iter->fd);

            fds_.erase(iter);
        }
    }


    void toggle_timeout(DBusTimeout* t)
    {
        //std::cout << "toggle_timeout" << std::endl;
        auto iter = std::find_if(fds_.begin(), fds_.end(), [t](auto& pfd){ return reinterpret_cast<int64_t>(dbus_timeout_get_data(t)) == pfd.fd; });
        if (iter != fds_.end())
        {
            if (dbus_timeout_get_enabled(t))
            {
                int interval_ms = dbus_timeout_get_interval(t);

                struct itimerspec spec = {
                    { interval_ms / 1000, (interval_ms % 1000) * 1000000L },
                    { interval_ms / 1000, (interval_ms % 1000) * 1000000L }
                };

                timerfd_settime(iter->fd, 0, &spec, 0);
            }
            else
                timerfd_settime(iter->fd, 0, 0, 0);
        }
    }


    int poll(int timeout)
    {
        //std::cout << "poll" << std::endl;
        if (::poll(&fds_[0], fds_.size(), timeout) > 0)
        {
            for(auto& pfd : fds_)
            {
                if (pfd.revents)
                {
                    auto result = watch_handlers_.equal_range(pfd.fd);
                    bool handled = false;

                    for(auto iter = result.first; iter != result.second; ++iter)
                    {
                       handled = true;   // ok, fd found...

                       if (pfd.revents & make_poll_events(dbus_watch_get_flags(iter->second)))
                       {
                           //std::cout << "handle watch" << std::endl;
                           dbus_watch_handle(iter->second, make_dbus_flags(pfd.revents));
                           break;
                        }
                    }

                    // must be a timeout
                    if (!handled)
                    {
                        auto t_iter = tm_handlers_.find(pfd.fd);

                        if (t_iter != tm_handlers_.end())
                        {
                            int64_t data;

                            int rc = ::read(pfd.fd, &data, sizeof(data));
                            (void)rc;

//                            std::cout << "handle timeout" << std::endl;
                            dbus_timeout_handle(t_iter->second);
                        }
                    }

                    break;
                }
            }
        }

        return 0;
    }


    void init(DBusConnection* conn)
    {
        dbus_connection_set_watch_functions(conn, &add_watch, &remove_watch, &toggle_watch, this, nullptr);
        dbus_connection_set_timeout_functions (conn, &add_timeout, &remove_timeout, &toggle_timeout, this, nullptr);
    }


    std::atomic_bool running_;
    std::vector<pollfd> fds_;

    std::multimap<int, DBusWatch*> watch_handlers_;
    std::map<int, DBusTimeout*> tm_handlers_;

    std::multimap<std::string, StubBase*> stubs_;
    std::map<std::string, int> signal_matches_;

    /// service registration's list
    std::set<std::string> busnames_;
};


DBusObjectPathVTable stub_v_table = { nullptr, &SkeletonBase::method_handler, nullptr, nullptr, nullptr, nullptr };


void enable_threads()
{
   dbus_threads_init_default();
}


void Dispatcher::init(int have_introspection, const char* busname)
{
   // compile check if stubs or skeletons are compiled with the settings
   // used for building the library
   assert(SIMPPL_HAVE_INTROSPECTION == have_introspection);

   d = new Dispatcher::Private;

   conn_ = nullptr;
   request_timeout_ = DBUS_TIMEOUT_USE_DEFAULT;

   DBusError err;
   dbus_error_init(&err);

   assert(!busname || !strncmp(busname, "bus:", 4));

   if (!busname || !strcmp(busname, "bus:session"))
   {
      conn_ = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
   }
   else
   {
       if (!strcmp(busname, "bus:system"))
       {
          conn_ = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err);
       }
       else
       {
          conn_ = dbus_connection_open_private(busname, &err);

          if (conn_)
          {
             dbus_error_init(&err);
             dbus_bus_register(conn_, &err);
          }
       }
   }

   assert(!dbus_error_is_set(&err));
   dbus_error_free(&err);

   dbus_connection_add_filter(conn_, &signal_filter, this, 0);

   // register for busname change notifications
   // response is (name, old, new)
   dbus_error_init(&err);
   dbus_bus_add_match(conn_, "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged',path='/org/freedesktop/DBus',sender='org.freedesktop.DBus'", &err);
   assert(!dbus_error_is_set(&err));
   dbus_error_free(&err);

   dbus_error_init(&err);
   std::ostringstream match_string;
   match_string
       << "type='signal',interface='org.simppl.dispatcher',member='notify_client',path='/org/simppl/dispatcher/" << ::getpid() << '/' << this << "'";

   dbus_bus_add_match(conn_, match_string.str().c_str(), &err);
   assert(!dbus_error_is_set(&err));
   dbus_error_free(&err);

   // call ListNames to get list of available services on the bus
   DBusMessage* msg = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

   dbus_error_init(&err);
   DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn_, msg, 1000, &err);
   assert(reply);
   dbus_error_free(&err);

   std::vector<std::string> busnames;

   DBusMessageIter iter;
   dbus_message_iter_init(reply, &iter);
   decode(iter, busnames);

   for(auto& busname : busnames)
   {
      if (busname[0] != ':')
         d->busnames_.insert(busname);
   }

   dbus_message_unref(msg);
   dbus_message_unref(reply);
}


Dispatcher::~Dispatcher()
{
   dbus_connection_close(conn_);
   dbus_connection_unref(conn_);

   delete d;
   d = 0;
}


void Dispatcher::notify_clients(const std::string& busname, ConnectionState state)
{
   auto range = d->stubs_.equal_range(busname);

   std::for_each(range.first, range.second, [state](auto& entry){
      entry.second->connection_state_changed(state);
   });
}


void Dispatcher::add_server(SkeletonBase& serv)
{
   DBusError err;
   dbus_error_init(&err);

   dbus_bus_request_name(conn_, serv.busname(), 0, &err);

   if (dbus_error_is_set(&err))
   {
      // FIXME make exception classes
      std::cerr << "dbus_bus_request_name - DBus error: " << err.name << ": " << err.message << std::endl;
      dbus_error_free(&err);
   }

   // register same path as busname, just with / instead of .
   dbus_error_init(&err);

   // register object path
   dbus_connection_try_register_object_path(conn_, serv.objectpath(), &stub_v_table, &serv, &err);

   if (dbus_error_is_set(&err))
   {
       std::cerr << "dbus_connection_register_object_path - DBus error: " << err.name << ": " << err.message << std::endl;
       dbus_error_free(&err);
   }

   serv.disp_ = this;
}


void Dispatcher::register_signal(StubBase& stub, ClientSignalBase& sigbase)
{
   register_signal_match(generate_matchstring(stub, sigbase.name()));
}


void Dispatcher::unregister_signal(StubBase& stub, ClientSignalBase& sigbase)
{
    unregister_signal_match(generate_matchstring(stub, sigbase.name()));
}


void Dispatcher::register_properties(StubBase& stub)
{
   register_signal_match(generate_property_matchstring(stub));
}


void Dispatcher::unregister_properties(StubBase& stub)
{
   unregister_signal_match(generate_property_matchstring(stub));
}


void Dispatcher::register_signal_match(const std::string& match_string)
{
   auto iter = d->signal_matches_.find(match_string);

   if (iter == d->signal_matches_.end())
   {
      DBusError err;
      dbus_error_init(&err);

      dbus_bus_add_match(conn_, match_string.c_str(), &err);
      assert(!dbus_error_is_set(&err));

      dbus_error_free(&err);

      d->signal_matches_[match_string] = 1;
   }
   else
      ++iter->second;
}


void Dispatcher::unregister_signal_match(const std::string& match_string)
{
   auto iter = d->signal_matches_.find(match_string);

   if (iter != d->signal_matches_.end())
   {
      if (--iter->second == 0)
      {
         DBusError err;
         dbus_error_init(&err);

         dbus_bus_remove_match(conn_, match_string.c_str(), &err);
         assert(!dbus_error_is_set(&err));

         dbus_error_free(&err);
      }
   }
}


DBusHandlerResult Dispatcher::try_handle_signal(DBusMessage* msg)
{
    // FIXME better check!
    if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_SIGNAL)
    {
        //std::cout << this << ": having signal '" << dbus_message_get_member(msg) << "'" << std::endl;

        if (!strcmp(dbus_message_get_member(msg), "notify_client"))
        {
           std::string busname;

           DBusMessageIter iter;
           dbus_message_iter_init(msg, &iter);

           decode(iter, busname);

           if (d->busnames_.find(busname) != d->busnames_.end())
                notify_clients(busname, ConnectionState::Connected);

           return DBUS_HANDLER_RESULT_HANDLED;
        }
        else if (!strcmp(dbus_message_get_member(msg), "NameOwnerChanged"))
        {
           // bus name, not interface

           std::string bus_name;
           std::string old_name;
           std::string new_name;

           DBusMessageIter iter;
           dbus_message_iter_init(msg, &iter);

           decode(iter, bus_name, old_name, new_name);

           if (bus_name[0] != ':')
           {
               if (old_name.empty())
               {
                   d->busnames_.insert(bus_name);
                   notify_clients(bus_name, ConnectionState::Connected);
               }
               else if (new_name.empty())
               {
                   d->busnames_.erase(bus_name);
                   notify_clients(bus_name, ConnectionState::Disconnected);
               }
           }

           return DBUS_HANDLER_RESULT_HANDLED;
        }

        // ordinary signals...

        // here we expect that pathname is the same as busname, just with / instead of .
        char originator[256];
        strncpy(originator, dbus_message_get_path(msg)+1, sizeof(originator));
        originator[sizeof(originator)-1] = '\0';

        char* p = originator;
        while(*++p)
        {
           if (*p == '/')
              *p = '.';
        }

        auto range = d->stubs_.equal_range(originator);
        if (range.first != range.second)
        {
            std::for_each(range.first, range.second, [msg](auto& entry){
                entry.second->try_handle_signal(msg);
            });

            return DBUS_HANDLER_RESULT_HANDLED;
        }
        // else
        //     nobody interested in signal - go on with other filters
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


void Dispatcher::stop()
{
   d->running_.store(false);
}


bool Dispatcher::is_running() const
{
   return d->running_.load();
}


void Dispatcher::add_client(StubBase& clnt)
{
   clnt.disp_ = this;

   //std::cout << "Adding stub: " << clnt.busname() << std::endl;
   d->stubs_.insert(std::make_pair(clnt.busname(), &clnt));

   // send connected request from event loop
   auto iter = d->busnames_.find(clnt.busname());
   if (iter != d->busnames_.end())
   {
      std::ostringstream objpath;
      objpath << "/org/simppl/dispatcher/" << ::getpid() << '/' << this;

      DBusMessage* msg = dbus_message_new_signal(objpath.str().c_str(), "org.simppl.dispatcher", "notify_client");

      DBusMessageIter iter;
      dbus_message_iter_init_append(msg, &iter);

      encode(iter, clnt.busname());

      dbus_connection_send(conn_, msg, nullptr);
      dbus_message_unref(msg);
   }
}


void Dispatcher::remove_client(StubBase& clnt)
{
   for(auto iter = d->stubs_.begin(); iter != d->stubs_.end(); ++iter)
   {
      if (&clnt == iter->second)
      {
         clnt.cleanup();

         d->stubs_.erase(iter);
         break;
      }
   }
}


void Dispatcher::dispatch()
{
    int rc;

    do
    {
       rc = dbus_connection_dispatch(conn_);
    }
    while(rc != DBUS_DISPATCH_COMPLETE);
}


int Dispatcher::step_ms(int timeout_ms)
{
#ifdef SIMPPL_USE_POLL
    d->poll(timeout_ms);

    dispatch();
#else
    dbus_connection_read_write_dispatch(conn_, 100);
#endif

   return 0;
}


void Dispatcher::init()
{
#ifdef SIMPPL_USE_POLL
   d->init(conn_);
#endif
}


int Dispatcher::run()
{
   init();

   d->running_.store(true);

   while(d->running_.load())
   {
       step_ms(100);
   }

   return 0;
}

}   // namespace dbus

}   // namespace simppl
