#include "simppl/dispatcher.h"

#include "simppl/detail/util.h"
#include "simppl/timeout.h"
#include "simppl/skeletonbase.h"

#define SIMPPL_DISPATCHER_CPP
#include "simppl/serverside.h"
#undef SIMPPL_DISPATCHER_CPP


using namespace std::placeholders;
using namespace std::literals::chrono_literals;


namespace
{

inline
std::chrono::steady_clock::time_point
get_lookup_duetime()
{
   return std::chrono::steady_clock::now() + 5s;
}   


struct BlockingResponseHandler0
{
   inline
   BlockingResponseHandler0(simppl::ipc::Dispatcher& disp, simppl::ipc::ClientResponse<>& r)
    : disp_(disp)
    , r_(r)
   {
      r_.handledBy(std::ref(*this));
   }
   
   inline
   void operator()(const simppl::ipc::CallState& state)
   {
      disp_.stop();
      r_.handledBy(std::nullptr_t());
            
      if (!state)
         state.throw_exception();
   }
   
   simppl::ipc::Dispatcher& disp_;
   simppl::ipc::ClientResponse<>& r_;
};


DBusHandlerResult signal_filter(DBusConnection* /*connection*/, DBusMessage* msg, void *user_data)
{
    simppl::ipc::Dispatcher* disp = (simppl::ipc::Dispatcher*)user_data;
    return disp->try_handle_signal(msg);
}

}   // namespace


// ---------------------------------------------------------------------------------------


namespace simppl
{

namespace ipc
{

DBusObjectPathVTable stub_v_table = { nullptr, &SkeletonBase::method_handler, nullptr, nullptr, nullptr, nullptr };


Dispatcher::Dispatcher()
 : running_(false)
 , conn_(nullptr)
 , request_timeout_(std::chrono::milliseconds::max())    // TODO move this to a config header, also other timeout defaults
{
   DBusError err;
   dbus_error_init(&err);

   conn_ = dbus_bus_get(DBUS_BUS_SESSION, &err);
   assert(!dbus_error_is_set(&err));
   
   dbus_connection_add_filter(conn_, &signal_filter, this, 0);
}


Dispatcher::~Dispatcher()
{
   dbus_connection_close(conn_);
}


DBusHandlerResult Dispatcher::try_handle_signal(DBusMessage* msg)
{
    if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_SIGNAL)
    {
        std::cout << "Having signal '" << dbus_message_get_interface(msg) << "/" << dbus_message_get_member(msg) << "'" << std::endl;
        
        // bus name, not interface
        if (!strcmp(dbus_message_get_member(msg), "NameAcquired"))
        {
           DBusMessageIter args;
           char* sigvalue = nullptr;
           
           dbus_message_iter_init(msg, &args);
           dbus_message_iter_get_basic(&args, &sigvalue);
           
           std::cout << "Name: " << sigvalue << std::endl;
        }
        
        auto iter = stubs_.find(dbus_message_get_interface(msg));
        
        if (iter != stubs_.end())
            return iter->second->try_handle_signal(msg);
    }
    else if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
    {
        // FIXME check for property Get return call...
        std::cout << "Method return: " << dbus_message_get_interface(msg) << ", " << dbus_message_get_member(msg) << std::endl;
    }   
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


void Dispatcher::waitForResponse(const detail::ClientResponseHolder& resp)
{
   assert(resp.r_);
   assert(!isRunning());
   
   ClientResponse<>* r = safe_cast<ClientResponse<>*>(resp.r_);
   assert(r);
   
   BlockingResponseHandler0 handler(*this, *r);
   
//   loop();
}


void Dispatcher::stop()
{
   running_.store(false);
}


bool Dispatcher::isRunning() const
{
   return running_.load();
}


void Dispatcher::addClient(StubBase& clnt)
{
   assert(!clnt.disp_);   // don't add it twice
   
   clnt.disp_ = this;
   
   // isn't this double the information?
   dynamic_cast<detail::BasicInterface*>(&clnt)->conn_ = conn_;
   
   // FIXME somehow integrate interface _and_ rolename
   stubs_.insert(std::make_pair(clnt.iface(), &clnt));
   
   /* if (isRunning())
         connect(clnt);
   }*/
}


void Dispatcher::removeClient(StubBase& clnt)
{
   for(auto iter = stubs_.begin(); iter != stubs_.end(); ++iter)
   {
      if (&clnt == iter->second)
      {
         stubs_.erase(iter);
         break;
      } 
   }
}


int Dispatcher::run()
{
   running_.store(true);
   
   while(dbus_connection_read_write_dispatch(conn_, -1) && running_.load());
}

}   // namespace ipc

}   // namespace simppl
