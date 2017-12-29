#ifndef SIMPPL_DISPATCHER_H
#define SIMPPL_DISPATCHER_H


#include <chrono>
#include <string>

#include <dbus/dbus.h>

#include "simppl/connectionstate.h"
#include "simppl/detail/constants.h"


namespace simppl
{

namespace dbus
{

// forward decls
struct StubBase;
struct SkeletonBase;
struct ClientSignalBase;
struct CallState;


void enable_threads();


/**
 * Event dispatcher. Actually the dispatcher wraps the DBusConnection 
 * and therefore is the access point for event loop integration or 
 * starting a self-contained event loop via the functions @c init(),
 * @c run and @c step_ms.
 */
struct Dispatcher
{
   friend struct StubBase;
   friend struct SkeletonBase;

   friend void dispatcher_add_stub(Dispatcher&, StubBase&);
   friend void dispatcher_add_skeleton(Dispatcher&, SkeletonBase&);

   Dispatcher(const Dispatcher&) = delete;
   Dispatcher& operator=(const Dispatcher&) = delete;

   /**
    * @param busname the busname to use, e.g. "bus:session" or "bus:system. nullptr means session.
    */
   inline
   Dispatcher(const char* busname = nullptr)
   {
      init(SIMPPL_HAVE_INTROSPECTION, busname);
   }

   ~Dispatcher();

   template<typename RepT, typename PeriodT>
   inline
   void set_request_timeout(std::chrono::duration<RepT, PeriodT> duration)
   {
      request_timeout_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
   }
   
   /**
    * Initialize a self-hosted eventloop. The function has only to be called
    * if the mainloop is proved
    */
   void init();
   
   /**
    * Start self-hosted eventloop. Will do all steps of initialing the
    * DBusConnection callouts for I/O and runs @c step_ms() in a loop.
    * until @c stop() is called.
    */
   int run();

   /**
    * Do some IO and dispatch the retrieved messages.
    */
   template<typename RepT, typename PeriodT>
   inline
   int step(std::chrono::duration<RepT, PeriodT> duration)
   {
       return step_ms(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
   }

   /**
    * Dispatch incoming messages. This function is called from self-hosted
    * mainloops and must be called after I/O operations on external mainloops.
    */
   void dispatch();

   /**
    * Stop self-hosted eventloop.
    */
   void stop();

   /**
    * Self hosted eventloop is running.
    */
   bool is_running() const;

   DBusHandlerResult try_handle_signal(DBusMessage* msg);

   void register_signal(StubBase& stub, ClientSignalBase& sigbase);
   void unregister_signal(StubBase& stub, ClientSignalBase& sigbase);

   void register_properties(StubBase& stub);
   void unregister_properties(StubBase& stub);

   inline
   int request_timeout() const
   {
      return request_timeout_;
   }

   inline
   DBusConnection& connection()
   {
      return *conn_;
   }

private:

   /** 
    * 'Inlined constructor' for checking match of introspection macro setting 
    * in user-code and compiled shared library.
    */
   void init(int have_introspection, const char* busname);

   void register_signal_match(const std::string& match_string);
   void unregister_signal_match(const std::string& match_string);

   /**
    * Add a client to the dispatcher. This is also necessary if blocking
    * stubs should be used.
    */
   void add_client(StubBase& clnt);

   /// Remove the client.
   void remove_client(StubBase& clnt);

   /// add a server
   void add_server(SkeletonBase& server);

   /// Do a single iteration on the self-hosted mainloop.
   int step_ms(int millis);

   void notify_clients(const std::string& boundname, ConnectionState state);

   DBusConnection* conn_;
   int request_timeout_;    ///< default request timeout in milliseconds

   struct Private;
   Private* d;
};


}   // namespace dbus

}   // namespace simppl


#endif   // SIMPPL_DISPATCHER_H
